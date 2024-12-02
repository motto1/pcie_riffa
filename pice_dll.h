#ifndef PICE_DLL_H
#define PICE_DLL_H

#include "pice_dll_global.h"
#include "riffa.h"
#include <QString>
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>

// 添加导出函数声明
#ifdef __cplusplus
extern "C" {
#endif

PICE_DLL_EXPORT void* CreatePiceDll();
PICE_DLL_EXPORT void DestroyPiceDll(void* instance);
PICE_DLL_EXPORT int CheckPcie(void* instance);
PICE_DLL_EXPORT int OpenPcie(void* instance);
PICE_DLL_EXPORT void ClosePcie(void* instance);
PICE_DLL_EXPORT bool FpgaFifoStart(void* instance, int value);
PICE_DLL_EXPORT void FpgaFifoOnce(void* instance);
PICE_DLL_EXPORT bool FpgaRead(void* instance, unsigned int* buffer, int timeout_ms);
PICE_DLL_EXPORT const char* GetPiceDllError(void* instance);
PICE_DLL_EXPORT bool IsConnected(void* instance);
PICE_DLL_EXPORT size_t GetCurrentFifoPos(void* instance);
PICE_DLL_EXPORT size_t GetNextReadPos(void* instance);
PICE_DLL_EXPORT void EnableRiffaLog(bool enable);
PICE_DLL_EXPORT void EnablePiceDllLog(bool enable);

#ifdef __cplusplus
}
#endif

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
    bool fpga_fifo_start(int value);  // 启动FIFO操作
    void fpga_fifo_once();  // 单次FIFO操作
    // void setFifoEnabled(bool enabled);  // 添加FIFO使能控制函数
    size_t getCurrentFifoPos() const { return current_buffer_pos; }  // 获取当前FIFO位置

    // FIFO读取函数
    bool fpga_read(unsigned int* read_buffer, int timeout_ms = 1000);  // 默认超时1秒
    size_t getNextReadPos() const { return next_read_pos; }  // 获取下一个读取位置

    // 日志控制函数
    static void enableRiffaLog(bool enable) { riffa_log_enabled = enable; }
    static bool isRiffaLogEnabled() { return riffa_log_enabled; }
    static void enablePiceDllLog(bool enable) { pice_dll_log_enabled = enable; }
    static bool isPiceDllLogEnabled() { return pice_dll_log_enabled; }

    // 版本信息相关函数
    struct VersionInfo {
        QString hardwareVersion;
        QString softwareVersion;
    };
    
    VersionInfo getVersionInfo();
    
signals:
    void logGenerated(const QString& message);
    void operationCompleted(qint64 pos, qint64 time);

    // 添加友元声，允许导出函数访问私有成员
    friend PICE_DLL_EXPORT void* CreatePiceDll();
    friend PICE_DLL_EXPORT void DestroyPiceDll(void* instance);
    friend PICE_DLL_EXPORT int CheckPcie(void* instance);
    friend PICE_DLL_EXPORT int OpenPcie(void* instance);
    friend PICE_DLL_EXPORT void ClosePcie(void* instance);
    friend PICE_DLL_EXPORT bool FpgaFifoStart(void* instance, int value);
    friend PICE_DLL_EXPORT void FpgaFifoOnce(void* instance);
    friend PICE_DLL_EXPORT bool FpgaRead(void* instance, unsigned int* buffer, int timeout_ms);
    friend PICE_DLL_EXPORT const char* GetPiceDllError(void* instance);
    friend PICE_DLL_EXPORT bool IsConnected(void* instance);
    friend PICE_DLL_EXPORT size_t GetCurrentFifoPos(void* instance);
    friend PICE_DLL_EXPORT size_t GetNextReadPos(void* instance);

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
    QElapsedTimer loopTimer;

    // FIFO相关变量
    unsigned char* fifo_buffer;      // 500M大内存缓冲区
    size_t buffer_size;              // 缓冲区总大小
    size_t current_buffer_pos;       // 当前写入位置
    static const size_t FIFO_BUFFER_SIZE = 500 * 1024 * 1024;  // 500MB
    int count;//

    // 辅助函数
    void setLastError(const QString& error);
    void cleanup();


    // 添加读取相关变量
    size_t next_read_pos;           // 下一个要读取的位置
    size_t last_package_size;       // 最后一次写入的包大小
    bool has_new_data;              // 是否有新数据标志

    int package_counter;  // 包计数器
    static const int MAX_PACKAGE_COUNT = 5000;  // 最大包数限制
    int read_count;  // 添加读取计数器


};

#endif // PICE_DLL_H
