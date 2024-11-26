#ifndef PICE_DLL_H
#define PICE_DLL_H

#include "pice_dll_global.h"
#include "riffa.h"
#include <QString>
#include <QObject>
#include <QElapsedTimer>
#include <QThread>

class PICE_DLL_EXPORT Pice_dll : public QObject
{
    Q_OBJECT
    
public:
    Pice_dll();
    ~Pice_dll();

    // FPGA操作相关函数
    int checkPcie();  // 检查PCIE设备
    int openPcie(unsigned int* buffer);   // 打开PCIE接口并设置buffer
    void closePcie(); // 关闭PCIE接口
    
    // FPGA数据收发函数
    bool prepare_fpga_send(int input_value); // 预备发送数据
    QPair<bool, qint64> fpga_send_recv();   // 执行发送接收
    
    // 获取状态信息
    QString getLastError() const;
    bool isConnected() const;
    qint64 getLastOperationTime() const;
    int sent;

    // 日志处理函数
    static void handleLog(const QString& message);

signals:
    void logGenerated(const QString& message);

private:
    static Pice_dll* instance;  // 添加静态成员变量

    // FPGA相关变量
    fpga_t* fpga;
    fpga_info_list fpga_pcie_info;
    int fpga_pcie_id;
    int fpga_pcie_chnl;
    unsigned int* pcie_Send_Buffer;  // 发送缓冲区指针
    unsigned int* pcie_recv_Buffer;  // 接收缓冲区指针
    int input_value;                 // 存储输入值
    QString lastError;
    bool connected;                  // 标记是否连接
    bool prepared;                   // 标记是否已经准备好发送
    
    // 计时和计数相关
    QElapsedTimer m_loopTimer;

    // 辅助函数
    void setLastError(const QString& error);
    void cleanup();
};

#endif // PICE_DLL_H
