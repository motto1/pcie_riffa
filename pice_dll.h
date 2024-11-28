#ifndef PICE_DLL_H
#define PICE_DLL_H

#include "pice_dll_global.h"
#include "riffa.h"
#include <QString>
#include <QObject>
#include <QElapsedTimer>
#include <QThread>

// 前向声明
class Pice_dll;

// FIFO线程类
class FifoThread : public QThread
{
    Q_OBJECT
public:
    FifoThread(Pice_dll* pcie, int value, QObject* parent = nullptr);
    void stop();

signals:
    void logMessage(const QString& message);
    void operationCompleted(qint64 pos, qint64 time);

protected:
    void run() override;

private:
    Pice_dll* m_pcie;
    int m_value;
    bool m_running;
    int sent;  // 添加sent变量
};

class PICE_DLL_EXPORT Pice_dll : public QObject
{
    Q_OBJECT
    
public:
    Pice_dll();
    ~Pice_dll();

    // FPGA操作相关函数
    int checkPcie();  // 检查PCIE设备
    int openPcie();   // 打开PCIE接口
    void closePcie(); // 关闭PCIE接口
    
    // FPGA数据收发函数
    bool prepare_fpga_send(int input_value); // 预备发送数据
    QPair<bool, qint64> fpga_send_recv(unsigned int* recv_buffer);   // 执行发送接收
    
    // 获取状态信息
    QString getLastError() const;
    bool isConnected() const;
    qint64 getLastOperationTime() const;
    int sent;

    // 日志处理函数
    static void handleLog(const QString& message);

    // FIFO操作函数
    bool fpga_fifo(int value);  // FIFO操作函数
    size_t getCurrentFifoPos() const { return current_buffer_pos; }  // 获取当前FIFO位置

    // 添加FIFO读取函数
    bool fpga_read(unsigned int* read_buffer, int timeout_ms = 1000);  // 默认超时1秒
    size_t getNextReadPos() const { return next_read_pos; }  // 获取下一个读取位置

    // 声明FifoThread为友元类，使其能访问私有成员
    friend class FifoThread;

    // 添加日志控制函数
    static void enableRiffaLog(bool enable) { riffa_log_enabled = enable; }
    static bool isRiffaLogEnabled() { return riffa_log_enabled; }
    static void enablePiceDllLog(bool enable) { pice_dll_log_enabled = enable; }
    static bool isPiceDllLogEnabled() { return pice_dll_log_enabled; }

    // 添加版本信息相关函数
    struct VersionInfo {
        QString hardwareVersion;
        QString softwareVersion;
    };
    
    VersionInfo getVersionInfo();

    void setFifoEnabled(bool enabled);  // 添加FIFO使能控制函数
    
signals:
    void logGenerated(const QString& message);

private:
    static Pice_dll* instance;  // 添加静态成员变量
    static bool riffa_log_enabled;  // 添加静态控制变量
    static bool pice_dll_log_enabled;  // PICE DLL日志控制

    // FPGA相关变量
    fpga_t* fpga;
    fpga_info_list fpga_pcie_info;
    int fpga_pcie_id;
    int fpga_pcie_chnl;
    unsigned int* pcie_Send_Buffer;  // 发送缓冲区指针
    int input_value;                 // 存储输入值
    QString lastError;
    bool connected;                  // 标记是否连接
    bool prepared;                   // 标记是否已经准备好发送
    
    // 计时和计数相关
    QElapsedTimer m_loopTimer;

    // FIFO相关变量
    unsigned char* fifo_buffer;      // 500M大内存缓冲区
    size_t buffer_size;              // 缓冲区总大小
    size_t current_buffer_pos;       // 当前写入位置
    static const size_t FIFO_BUFFER_SIZE = 500 * 1024 * 1024;  // 500MB

    // 辅助函数
    void setLastError(const QString& error);
    void cleanup();

    FifoThread* fifo_thread;  // 添加FIFO线程指针

    // 添加读取相关变量
    size_t next_read_pos;           // 下一个要读取的位置
    size_t last_package_size;       // 最后一次写入的包大小
    bool has_new_data;              // 是否有新数据标志

    int package_counter;  // 包计数器
    static const int MAX_PACKAGE_COUNT = 5000;  // 最大包数限制
    int read_count;  // 添加读取计数器


};

#endif // PICE_DLL_H
