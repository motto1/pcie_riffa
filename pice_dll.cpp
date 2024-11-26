#include "pice_dll.h"
#include <QDebug>
#include "riffa.h"

// 初始化静态成员变量
Pice_dll* Pice_dll::instance = nullptr;

// 在文件开头添加
void logMessage(const QString& message) {
    Pice_dll::handleLog(message);
}

void Pice_dll::handleLog(const QString& message) {
    if (instance) {
        emit instance->logGenerated(message);
    }
    qDebug() << "RIFFA:" << message;
}

Pice_dll::Pice_dll() : 
    fpga(nullptr),
    fpga_pcie_id(0),
    fpga_pcie_chnl(0),
    pcie_Send_Buffer(nullptr),
    pcie_recv_Buffer(nullptr),
    connected(false)
{
    instance = this;  // 设置实例指针
    m_loopTimer.start();
}

Pice_dll::~Pice_dll()
{
    if (instance == this) {
        instance = nullptr;  // 清除实例指针
    }
    cleanup();
}

int Pice_dll::checkPcie()
{
    
    if (fpga_list(&fpga_pcie_info) != 0) {
        setLastError("FPGA列表获取失败");
        return 0;
    }
    
    if(fpga_pcie_info.num_fpgas == 0) {
        setLastError("未检测到PCIE设备");
        return 0;
    }
    
    return fpga_pcie_info.num_fpgas;
}

int Pice_dll::openPcie(unsigned int* buffer)
{
    // 如果已经连接,先关闭
    if(connected) {
        closePcie();
    }

    if (!buffer) {
        setLastError("无效的buffer参数");
        return 1;
    }

    fpga_pcie_id = 0;
    fpga_pcie_chnl = 0;
    
    // 打开FPGA设备
    fpga = fpga_open(fpga_pcie_id);
    if(!fpga) {
        setLastError("FPGA设备打开失败");
        return 1;
    }

    // 置接收缓冲区
    pcie_recv_Buffer = buffer;

    // 分配发送缓冲区
    pcie_Send_Buffer = (unsigned int*)malloc(8*8);
    if(!pcie_Send_Buffer) {
        setLastError("发送缓冲区分配失败");
        cleanup();
        return 1;
    }

    connected = true;
    return 0;
}

void Pice_dll::closePcie()
{
    cleanup();
    connected = false;
}

bool Pice_dll::prepare_fpga_send(int value)
{
    if (!connected || !pcie_Send_Buffer) {
        setLastError("设备未连接或缓冲区未初始化");
        return false;
    }

    input_value = value;  // 保存输入值

    // 获取输入值的高16位和低16位
    uint16_t high_word = (input_value >> 16) & 0xFFFF;
    uint16_t low_word = input_value & 0xFFFF;
    
    // 拼接赋值
    pcie_Send_Buffer[0] = (low_word << 16) | 0x013c;
    pcie_Send_Buffer[1] = (0x3e00 << 16) | high_word;
    
    qDebug() << QString("初始化发送缓冲区 - Buffer[0]:0x%1, Buffer[1]:0x%2")
        .arg(pcie_Send_Buffer[0], 8, 16, QChar('0'))
        .arg(pcie_Send_Buffer[1], 8, 16, QChar('0'));
    
    sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    if (sent == 0) {
        setLastError("FPGA预备发送失败");
        return false;
    }

    prepared = true;
    return true;
}

QPair<bool, qint64> Pice_dll::fpga_send_recv()
{
    if (!prepared) {
        setLastError("未执行预备发送操作");
        return qMakePair(false, 0);
    }
    m_loopTimer.start();  // 启动计时器

    if (sent == 0) {
        setLastError("FPGA发送失败");
        return qMakePair(false, 0);
    }
    
    // 接收数据
    int rece = fpga_recv(fpga, fpga_pcie_chnl, pcie_recv_Buffer, input_value * 6, 2500);
    if (rece != 0) {
        // 发送继续命令
        pcie_Send_Buffer[0] = 0x0000023c;
        pcie_Send_Buffer[1] = 0x3e000000;
        sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    } 
    else {
        setLastError("FPGA接收失败");
        return qMakePair(false, 0);
    }

    // // 等待指定时间
    // QElapsedTimer waitTimer;
    // waitTimer.start();
    // while(waitTimer.nsecsElapsed() < 10000 * input_value - 4 * 10000) {
    //     QThread::yieldCurrentThread();
    // }

    // 更新计时
    qint64 currentElapsedMicros = m_loopTimer.nsecsElapsed() / 1000;
    // prepared = false;  // 重置准备状态
    return qMakePair(true, currentElapsedMicros);
}

QString Pice_dll::getLastError() const
{
    return lastError;
}

bool Pice_dll::isConnected() const
{
    return connected;
}





void Pice_dll::setLastError(const QString& error)
{
    lastError = error;
    qDebug() << "PICE DLL Error:" << error;
}

void Pice_dll::cleanup()
{
    if(pcie_Send_Buffer) {
        free(pcie_Send_Buffer);
        pcie_Send_Buffer = nullptr;
    }
    
    // 不再释放 pcie_recv_Buffer，因为它是外部传入的
    pcie_recv_Buffer = nullptr;
    
    if(fpga) {
        fpga_close(fpga);
        fpga = nullptr;
    }
}
