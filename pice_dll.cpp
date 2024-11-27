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
    connected(false),
    fifo_buffer(nullptr),
    buffer_size(FIFO_BUFFER_SIZE),
    current_buffer_pos(0),
    fifo_thread(nullptr),
    next_read_pos(0),
    last_package_size(0),
    has_new_data(false)
{
    instance = this;  // 设置实例指针
    m_loopTimer.start();
    
    // 分配500M内存
    fifo_buffer = (unsigned char*)malloc(buffer_size);
    if (!fifo_buffer) {
        setLastError("FIFO缓冲区分配失败");
        return;
    }
    memset(fifo_buffer, 0, buffer_size);
}

Pice_dll::~Pice_dll()
{
    if (instance == this) {
        instance = nullptr;  // 清除实例指针
    }
    cleanup();
    
    if (fifo_buffer) {
        free(fifo_buffer);
        fifo_buffer = nullptr;
    }

    if (fifo_thread) {
        fifo_thread->stop();
        fifo_thread->wait();
        delete fifo_thread;
    }
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

int Pice_dll::openPcie()
{
    // 如果已经连接,先关闭
    if(connected) {
        closePcie();
    }

    fpga_pcie_id = 0;
    fpga_pcie_chnl = 0;
    
    // 打开FPGA设备
    fpga = fpga_open(fpga_pcie_id);
    if(!fpga) {
        setLastError("FPGA设备打开失败");
        return 1;
    }

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

QPair<bool, qint64> Pice_dll::fpga_send_recv(unsigned int* recv_buffer)
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
    
    // 接收数据到传入的缓冲区
    int rece = fpga_recv(fpga, fpga_pcie_chnl, recv_buffer, input_value * 6, 2500);
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
    
    if(fpga) {
        fpga_close(fpga);
        fpga = nullptr;
    }
    
    current_buffer_pos = 0;  // 重置FIFO位置
    next_read_pos = 0;
    last_package_size = 0;
    has_new_data = false;
}

bool Pice_dll::fpga_fifo(int value)
{
    if (!connected || !pcie_Send_Buffer || !fifo_buffer) {
        setLastError("设备未连接或缓冲区未初始化");
        return false;
    }
    input_value = value;  // 保存输入值
    // 停止现有的FIFO线程
    if (fifo_thread) {
        fifo_thread->stop();
        fifo_thread->wait();
        delete fifo_thread;
    }
    // 获取输入值的高16位和低16位
    uint16_t high_word = (input_value >> 16) & 0xFFFF;
    uint16_t low_word = input_value & 0xFFFF;
    
    // 拼接赋值
    pcie_Send_Buffer[0] = (low_word << 16) | 0x013c;
    pcie_Send_Buffer[1] = (0x3e00 << 16) | high_word;
    
    sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    if (sent == 0) {
        setLastError("FPGA预备发送失败");
        return false;
    }
    // 创建并启动新的FIFO线程
    fifo_thread = new FifoThread(this, value, this);
    connect(fifo_thread, &FifoThread::logMessage, 
            this, &Pice_dll::logGenerated);
    connect(fifo_thread, &FifoThread::operationCompleted,
            [this](qint64 pos, qint64 time) {
                logMessage(QString("FIFO位置: %1 MB, 耗时: %2 微秒")
                    .arg(pos / (1024 * 1024))
                    .arg(time));
            });

    fifo_thread->start();
    return true;
}

// 添加FIFO线程类的实现
FifoThread::FifoThread(Pice_dll* pcie, int value, QObject* parent)
    : QThread(parent), m_pcie(pcie), m_value(value), m_running(true)
{
}

void FifoThread::stop()
{
    m_running = false;
}

void FifoThread::run()
{
    while(m_running) {
        if (sent == 0) {
            emit logMessage("FPGA发送失败");
            break;
        }

        // 计算本次接收数据的大小
        size_t recv_size = m_value * 6 * sizeof(unsigned int);
        
        // 检查缓冲区是否足够
        if (m_pcie->current_buffer_pos + recv_size > m_pcie->buffer_size) {
            // 缓冲区已满，从头开始写入
            m_pcie->current_buffer_pos = 0;
            emit logMessage("FIFO缓冲区已满，从头开始写入");
        }
        
        // 接收数据到FIFO缓冲区的当前位置
        int rece = fpga_recv(m_pcie->fpga, m_pcie->fpga_pcie_chnl, 
                            (unsigned int*)(m_pcie->fifo_buffer + m_pcie->current_buffer_pos), 
                            m_value * 6, 2500);
                            
        if (rece != 0) {
            // 更新写入位置和包大小信息
            m_pcie->last_package_size = recv_size;
            m_pcie->has_new_data = true;
            m_pcie->current_buffer_pos += recv_size;
            
            // 发送继续命令
            m_pcie->pcie_Send_Buffer[0] = 0x0000023c;
            m_pcie->pcie_Send_Buffer[1] = 0x3e000000;
            sent = fpga_send(m_pcie->fpga, m_pcie->fpga_pcie_chnl, 
                           m_pcie->pcie_Send_Buffer, 2, 0, 1, 25000);
            
            // 每10000次记录一次位置信息
            static int count = 0;
            count++;
            if (count % 10000 == 0) {
                emit operationCompleted(m_pcie->current_buffer_pos, 
                                     m_pcie->m_loopTimer.nsecsElapsed() / 1000);
            }
        } 
        else {
            emit logMessage("FPGA接收失败");
            break;
        }
    }
    
    emit logMessage("FIFO线程结束");
}

bool Pice_dll::fpga_read(unsigned int* read_buffer, int timeout_ms)
{
    if (!fifo_buffer || !read_buffer) {
        setLastError("缓冲区未初始化或读取缓冲区无效");
        return false;
    }

    // 检查是否超出缓冲区范围
    if (next_read_pos >= buffer_size) {
        next_read_pos = 0;  // 回到缓冲区开始位置
    }

    // 计算等待超时时间
    QElapsedTimer timer;
    timer.start();

    // 等待数据就绪
    while (!has_new_data || next_read_pos >= current_buffer_pos) {
        if (timer.elapsed() > timeout_ms) {
            setLastError("读取超时");
            return false;
        }
        QThread::msleep(1);  // 短暂休眠，避免过度占用CPU
    }

    // 确保有足够的数据可读
    if (next_read_pos + last_package_size > current_buffer_pos) {
        setLastError("数据不完整");
        return false;
    }

    // 复制数据到读取缓冲区
    memcpy(read_buffer, fifo_buffer + next_read_pos, last_package_size);

    // 更新读取位置
    next_read_pos += last_package_size;

    // 记录读取操作
    logMessage(QString("从FIFO位置 %1 读取了 %2 字节数据")
        .arg(next_read_pos - last_package_size)
        .arg(last_package_size));

    return true;
}
