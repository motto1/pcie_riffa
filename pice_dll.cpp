#include "pice_dll.h"
#include <QDebug>
#include "riffa.h"

// 初始化静态成员变量
Pice_dll* Pice_dll::instance = nullptr;
bool Pice_dll::riffa_log_enabled = false;    // 默认关闭RIFFA日志
bool Pice_dll::pice_dll_log_enabled = true;  // 默认开启PICE DLL日志

// RIFFA的日志函数
void logMessage(const QString& message) {
    // 只有在启用RIFFA日志时才输出
    if (Pice_dll::isRiffaLogEnabled()) {
        Pice_dll::handleLog("RIFFA: " + message);
    }
}

// PICE DLL的日志处理
void Pice_dll::handleLog(const QString& message) {
    if (instance) {
        if (message.startsWith("RIFFA:")) {
            // RIFFA日志，根据RIFFA日志开关控制
            if (isRiffaLogEnabled()) {
                emit instance->logGenerated(message);
            }
        } else {
            // PICE DLL日志，根据PICE DLL日志开关控制
            if (isPiceDllLogEnabled()) {
                // 如果消息已经有前缀，直接发送，否则添加前缀
                if (message.startsWith("Pice_dll:")) {
                    emit instance->logGenerated(message);
                } else {
                    emit instance->logGenerated("Pice_dll: " + message);
                }
            }
        }
    }
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
    has_new_data(false),
    package_counter(0),
    read_count(0)
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
        setLastError("设备已连接,请先关闭再重新连接");
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
    if (isPiceDllLogEnabled()) {
        qDebug() << "Pice_dll Error:" << error;
    }
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
    package_counter = 0;  // 重置包计数器
    read_count = 0;  // 重置读取计数器
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
    int count = 0;
    QElapsedTimer loopTimer;
    while(m_running) {
        // 检查连接状态，如果未连接则停止线程
        if (!m_pcie->isConnected()) {
            emit logMessage("设备未连接，停止FIFO_riffa线程");
            break;
        }

        if (sent == 0) {
            emit logMessage("FPGA发送失败");
            break;
        }
        loopTimer.start(); // 开始计时  
        // 计算本次接收数据的大小
        size_t recv_size = m_value * 6 * sizeof(unsigned int);
        
        // 检查缓冲区是否有足够空间容纳完整的包
        if (m_pcie->current_buffer_pos + recv_size > m_pcie->buffer_size) {
            // 如果剩余空间不足一个完整的包，直接跳到缓冲区开始
            size_t remaining_space = m_pcie->buffer_size - m_pcie->current_buffer_pos;
            if (remaining_space < recv_size) {
                // 在跳转前记录日志
                emit logMessage(QString("缓冲区剩余空间(%1 字节)不足一个完整包(%2 字节)，跳转到缓冲区起始位置")
                    .arg(remaining_space)
                    .arg(recv_size));
                
                // 用0填充剩余空间，保持对齐
                memset(m_pcie->fifo_buffer + m_pcie->current_buffer_pos, 0, remaining_space);
                m_pcie->current_buffer_pos = 0;
            }
        }

        // 接收数据到FIFO缓冲区的当前位置
        int rece = fpga_recv(m_pcie->fpga, m_pcie->fpga_pcie_chnl, 
                            (unsigned int*)(m_pcie->fifo_buffer + m_pcie->current_buffer_pos), 
                            m_value * 6, 2500);
                            
        if (rece != 0) {
            count++;
            // 更新包计数器
            m_pcie->package_counter++;
            if (m_pcie->package_counter >= m_pcie->MAX_PACKAGE_COUNT) {
                emit logMessage(QString("组计数器达到上限(%1)，停止接收")
                    .arg(m_pcie->package_counter));
                break;
            }

            // 每10000次打印一次数据和循环时间
            if(count % 10000 == 0) {
                unsigned int* recv_data = (unsigned int*)(m_pcie->fifo_buffer + m_pcie->current_buffer_pos);
                QString data_str;
                for(int i = 0; i < m_value * 6 && i < 10; i++) {
                    data_str += QString("0x%1 ").arg(recv_data[i], 8, 16, QChar('0'));
                }
                if(m_value * 6 > 10) {
                    data_str += "...";
                }
                emit logMessage(QString("第 %1 次接收到数据: %2")
                    .arg(count)
                    .arg(data_str));
            }
            // 更新写入位置和包大小信息
            m_pcie->last_package_size = recv_size;
            m_pcie->has_new_data = true;
            m_pcie->current_buffer_pos += recv_size;
            
            // 发送继续命令
            m_pcie->pcie_Send_Buffer[0] = 0x0000023c;
            m_pcie->pcie_Send_Buffer[1] = 0x3e000000;
            sent = fpga_send(m_pcie->fpga, m_pcie->fpga_pcie_chnl, 
                           m_pcie->pcie_Send_Buffer, 2, 0, 1, 25000);
            
            // 等待指定时间
            QElapsedTimer waitTimer;
            waitTimer.start();
            while(waitTimer.nsecsElapsed() < 10000 * m_value - 4 * 10000) {
                QThread::yieldCurrentThread();
            }

            qint64 loopTime = loopTimer.nsecsElapsed() / 1000; // 转换为微秒
            // 每10000次记录一次位置信息
            if (count % 10000 == 0) {
                emit operationCompleted(m_pcie->current_buffer_pos, 
                                     m_pcie->m_loopTimer.nsecsElapsed() / 1000);
                
                emit logMessage(QString("缓冲区状态 - 当前位置: %1 MB, 总大小: %2 MB, 剩余空间: %3 MB, 未读包数: %4，本次循环耗时: %5 微秒")
                    .arg(m_pcie->current_buffer_pos / (1024 * 1024))
                    .arg(m_pcie->buffer_size / (1024 * 1024))
                    .arg((m_pcie->buffer_size - m_pcie->current_buffer_pos) / (1024 * 1024))
                    .arg(m_pcie->package_counter)
                    .arg(loopTime));
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

    // 计算等待超时时间
    QElapsedTimer timer;
    timer.start();

    // 等待数据就绪
    while (!has_new_data || next_read_pos >= current_buffer_pos) {
        // 检查是否需要回环到缓冲区开始
        if (current_buffer_pos < next_read_pos) {
            // 写入位置小于读取位置，说明发生了回环
            next_read_pos = 0;
            handleLog("读取位置重置到缓冲区起始位置");
            continue;
        }

        if (timer.elapsed() > timeout_ms) {
            setLastError("读取超时");
            return false;
        }
    }

    // 检查是否需要处理回环情况
    if (next_read_pos + last_package_size > buffer_size) {
        // 如果当前包会超出缓冲区末尾，重置到开始位置
        next_read_pos = 0;
        handleLog("包跨越缓冲区边界，读取位置重置到起始位置");
    }

    // 确保有足够的数据可读
    if (next_read_pos + last_package_size > current_buffer_pos) {
        setLastError("数据不完整");
        return false;
    }

    // 复制数据到读取缓冲区
    memcpy(read_buffer, fifo_buffer + next_read_pos, last_package_size);

    // 每10000次读取打印一次数据
    read_count++;
    if(read_count % 10000 == 0) {
        QString dataStr = QString("第 %1 次读取的数据: ").arg(read_count);
        for(size_t i = 0; i < last_package_size/sizeof(unsigned int) && i < 8; i++) {
            dataStr += QString("0x%1 ").arg(read_buffer[i], 8, 16, QChar('0'));
        }
        if(last_package_size/sizeof(unsigned int) > 8) {
            dataStr += "...";
        }
        handleLog(dataStr);
    }

    // 更新读取位置
    next_read_pos += last_package_size;

    // 更新包计数器
    package_counter--;

    // 每10000次读取打印一次读取操作信息
    if(read_count % 10000 == 0) {
        handleLog(QString("从FIFO位置 %1 读取了 %2 字节数据，剩余未读组数: %3")
            .arg(next_read_pos - last_package_size)
            .arg(last_package_size)
            .arg(package_counter));
    }

    return true;
}

Pice_dll::VersionInfo Pice_dll::getVersionInfo()
{
    VersionInfo info;
    
    // if (!connected || !pcie_Send_Buffer) {
    //     setLastError("设备未连接或缓冲区未初始化");
    //     return info;
    // }

    // // 发送获取版本信息的命令
    // pcie_Send_Buffer[0] = 0x0000013c;  // 版本信息命令
    // pcie_Send_Buffer[1] = 0x3e000000;
    
    // int sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    // if (sent == 0) {
    //     setLastError("发送版本查询命令失败");
    //     return info;
    // }

    // // 接收版本信息
    // unsigned int version_buffer[4];  // 用于接收版本信息的缓冲区
    // int rece = fpga_recv(fpga, fpga_pcie_chnl, version_buffer, 4, 2500);
    // if (rece != 0) {
    //     // 解析硬件版本
    //     unsigned int hw_major = (version_buffer[0] >> 24) & 0xFF;
    //     unsigned int hw_minor = (version_buffer[0] >> 16) & 0xFF;
    //     unsigned int hw_revision = version_buffer[0] & 0xFFFF;
    //     info.hardwareVersion = QString("v%1.%2.%3")
    //         .arg(hw_major)
    //         .arg(hw_minor)
    //         .arg(hw_revision);

        // // 解析软件版本
        // unsigned int sw_major = (version_buffer[1] >> 24) & 0xFF;
        // unsigned int sw_minor = (version_buffer[1] >> 16) & 0xFF;
        // unsigned int sw_revision = version_buffer[1] & 0xFFFF;
        // info.softwareVersion = QString("v%1.%2.%3")
        //     .arg(sw_major)
        //     .arg(sw_minor)
        //     .arg(sw_revision);
        //     // 直接设置软件版本为0.1
        info.softwareVersion = QString("v0.1");
        info.hardwareVersion = QString("暂无");

    // 记录日志
    handleLog(QString("获取版本信息成功 - 软件版本: %1").arg(info.softwareVersion));

    return info;
}

void Pice_dll::setFifoEnabled(bool enabled) {
    if (!enabled && fifo_thread) {
        fifo_thread->stop();  // 停止FIFO线程
        fifo_thread->wait();
        delete fifo_thread;
        fifo_thread = nullptr;
    }
}
