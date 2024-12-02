#include "pice_dll.h"
#include "riffa.h"
#include <windows.h>
#include <sstream>
#include <iomanip>

// 静态成员定义
#ifdef PICE_DLL_LIB
Pice_dll* Pice_dll::instance = nullptr;
bool Pice_dll::riffa_log_enabled = false;
bool Pice_dll::pice_dll_log_enabled = true;
LogCallback Pice_dll::log_callback = nullptr;
#endif

// RIFFA的日志函数
void logMessage(const std::string& message) {
    if (Pice_dll::getInstance()) {
        Pice_dll::getInstance()->handleLog(message);
    }
}

// PICE DLL的日志处理
void Pice_dll::handleLog(const std::string& message) {
    // 构造完整的日志消息
    std::string fullMessage;
    
    // 处理RIFFA日志
    if (message.find("RIFFA:") == 0) {
        if (isRiffaLogEnabled()) {
            fullMessage = message;
        } else {
            return;
        }
    }
    // 处理PICE DLL日志
    else {
        if (isPiceDllLogEnabled()) {
            if (message.find("Pice_dll:") == 0) {
                fullMessage = message;
            } else {
                fullMessage = "Pice_dll: " + message;
            }
        } else {
            return;
        }
    }

    // 输出到控制台
    printf("%s\n", fullMessage.c_str());

    // 如果设置了回调函数，调用回调
    if (log_callback) {
        log_callback(fullMessage.c_str());
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
    next_read_pos(0),
    last_package_size(0),
    has_new_data(false),
    package_counter(0),
    read_count(0),
    count(0)
{
    setInstance(this);  // 使用静态方法设置实例
    
    // 初始化高精度计时器
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start_time);
    last_time = start_time;
    
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
    if (getInstance() == this) {
        setInstance(nullptr);
    }
    cleanup();
    
    if (fifo_buffer) {
        free(fifo_buffer);
        fifo_buffer = nullptr;
    }
}

// 获取当前时间戳（微秒）
int64_t Pice_dll::getCurrentTimeMicros() {
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return (current.QuadPart - start_time.QuadPart) * 1000000 / frequency.QuadPart;
}

// 重置计时器
void Pice_dll::resetTimer() {
    QueryPerformanceCounter(&start_time);
    last_time = start_time;
}

// 获取距离上次计时的时间（微秒）
int64_t Pice_dll::getElapsedMicros() {
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    int64_t elapsed = (current.QuadPart - last_time.QuadPart) * 1000000 / frequency.QuadPart;
    last_time = current;
    return elapsed;
}

int Pice_dll::checkPcie()
{
    std::stringstream logMsg;
    
    // 检查FPGA列表
    if (fpga_list(&fpga_pcie_info) != 0) {
        setLastError("FPGA列表获取失败");
        return 0;
    }
    
    // 检查设备数量
    if(fpga_pcie_info.num_fpgas == 0) {
        setLastError("未检测到PCIE设备");
        return 0;
    }
    
    // 记录找到的设备数量
    logMsg << "找到 " << fpga_pcie_info.num_fpgas << " 个PCIE设备";
    handleLog(logMsg.str());
    
    return fpga_pcie_info.num_fpgas;
}

int Pice_dll::openPcie()
{
    // 检查是否已连接
    if(connected) {
        setLastError("设备已连接,请先关闭再重新连接");
        return 1;
    }

    // 初始化设备ID和通道
    fpga_pcie_id = 0;
    fpga_pcie_chnl = 0;
    
    // 打开FPGA设备
    handleLog("正在打开FPGA设备...");
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

    // 设置连接状态并记录日志
    connected = true;
    handleLog("FPGA设备打开成功");
    
    return 0;
}

void Pice_dll::closePcie()
{
    if (connected) {
        handleLog("正在关闭FPGA设备...");
        cleanup();
        connected = false;
        handleLog("FPGA设备已关闭");
    }
}



void Pice_dll::setLastError(const std::string& error)
{
    lastError = error;
    if (isPiceDllLogEnabled()) {
        handleLog("Error: " + error);
    }
}

void Pice_dll::cleanup()
{
    // 释放发送缓冲区
    if(pcie_Send_Buffer) {
        free(pcie_Send_Buffer);
        pcie_Send_Buffer = nullptr;
    }
    
    // 关闭FPGA设备
    if(fpga) {
        fpga_close(fpga);
        fpga = nullptr;
    }
    
    // 重置所有状态
    current_buffer_pos = 0;
    next_read_pos = 0;
    last_package_size = 0;
    has_new_data = false;
    package_counter = 0;
    read_count = 0;
    prepared = false;
}

bool Pice_dll::fpga_fifo_start(int value)
{
    std::stringstream logMsg;
    
    // 检查设备和缓冲区状态
    if (!connected || !pcie_Send_Buffer || !fifo_buffer) {
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
    
    // 记录初始化信息
    logMsg << "初始化发送缓冲区 - Buffer[0]:0x" 
           << std::hex << std::setfill('0') << std::setw(8) << pcie_Send_Buffer[0]
           << ", Buffer[1]:0x" 
           << std::hex << std::setfill('0') << std::setw(8) << pcie_Send_Buffer[1];
    handleLog(logMsg.str());
    
    // 发送初始化命令
    sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    if (sent == 0) {
        setLastError("FPGA预备发送失败");
        return false;
    }
    // // 创建并启动新的FIFO线程
    // fifo_thread = new FifoThread(this, value, this);
    // connect(fifo_thread, &FifoThread::logMessage, 
    //         this, &Pice_dll::logGenerated);
    // connect(fifo_thread, &FifoThread::operationCompleted,
    //         [this](qint64 pos, qint64 time) {
    //             logMessage(QString("FIFO位置: %1 MB, 耗时: %2 微秒")
    //                 .arg(pos / (1024 * 1024))
    //                 .arg(time));
    //         });

    // fifo_thread->start();
    count = 0;
    // 重置计时器
    resetTimer();
    handleLog("FIFO操作初始化成功");
    
    return true;
}



void Pice_dll::fpga_fifo_once()
{
    std::stringstream logMsg;
    
    // 检查连接状态
    if (!isConnected()) {
        handleLog("设备未连接，停止FIFO操作");
        return;
    }

    if (sent == 0) {
        handleLog("FPGA发送失败");
        return;
    }

    // 开始计时
    resetTimer();

    // 计算本次接收数据的大小
    size_t recv_size = input_value * 6 * sizeof(unsigned int);
    
    // 检查缓冲区是否有足够空间容纳完整的包
    if (current_buffer_pos + recv_size > buffer_size) {
        // 如果剩余空间不足一个完整的包，直接跳到缓冲区开始
        size_t remaining_space = buffer_size - current_buffer_pos;
        if (remaining_space < recv_size) {
            logMsg.str("");
            logMsg << "缓冲区剩余空间(" << remaining_space 
                  << " 字节)不足一个完整包(" << recv_size 
                  << " 字节)，跳转到缓冲区起始位置";
            handleLog(logMsg.str());
            
            // 用0填充剩余空间，保持对齐
            memset(fifo_buffer + current_buffer_pos, 0, remaining_space);
            current_buffer_pos = 0;
        }
    }

    // 接收数据到FIFO缓冲区当前位置
    int rece = fpga_recv(fpga, fpga_pcie_chnl, 
                        (unsigned int*)(fifo_buffer + current_buffer_pos), 
                        input_value * 6, 2500);
                        
    if (rece != 0) {
        count++;
        // 更新包计数器
        package_counter++;
        if (package_counter >= MAX_PACKAGE_COUNT) {
            logMsg.str("");
            logMsg << "组计数器达到上限(" << MAX_PACKAGE_COUNT << ")，停止接收";
            handleLog(logMsg.str());
            return;
        }

        // 每10000次打印一次数据
        if(count % 10000 == 0 || count < 1000) {
            logMsg.str("");
            logMsg << "第 " << count << " 次接收到数据: ";
            unsigned int* recv_data = (unsigned int*)(fifo_buffer + current_buffer_pos);
            for(int i = 0; i < input_value * 6; i++) {
                logMsg << "0x" << std::hex << std::setfill('0') << std::setw(8) 
                      << recv_data[i] << " ";
            }
            handleLog(logMsg.str());
        }

        // 更新写入位置和包大小信息
        last_package_size = recv_size;
        has_new_data = true;
        current_buffer_pos += recv_size;

        // 等待指定时间
        int target_micros = 10000 * input_value - 4 * 10000;
        waitMicroseconds(target_micros);
        
        // 发送继续命令
        pcie_Send_Buffer[0] = 0x0000023c;
        pcie_Send_Buffer[1] = 0x3e000000;
        sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);

        // 记录循环时间和状态
        int64_t loopTime = getElapsedMicros();
        if(count % 10000 == 0 || count < 1000) {
            logMsg.str("");
            logMsg << "缓冲区状态 - 当前位置: " << (current_buffer_pos / (1024 * 1024))
                  << " MB, 总大小: " << (buffer_size / (1024 * 1024))
                  << " MB, 剩余空间: " << ((buffer_size - current_buffer_pos) / (1024 * 1024))
                  << " MB, 未读包数: " << package_counter
                  << "，本次循环耗时: " << loopTime << " 微秒";
            handleLog(logMsg.str());
        }
    } else {
        handleLog("FPGA接收失败");
    }
}

bool Pice_dll::fpga_read(unsigned int* read_buffer, int timeout_ms)
{
    std::stringstream logMsg;
    
    if (!fifo_buffer || !read_buffer) {
        setLastError("缓冲区未初始化或读取缓冲区无效");
        return false;
    }

    // 获取开始时间
    LARGE_INTEGER start_wait;
    QueryPerformanceCounter(&start_wait);
    
    // 等待数据就绪
    while (!has_new_data || next_read_pos >= current_buffer_pos) {
        // 检查是否需要回环到缓冲区开始
        if (current_buffer_pos < next_read_pos) {
            // 写入位置小于读取位置，说明发生了回环
            next_read_pos = 0;
            handleLog("读取位置重置到缓冲区起始位置");
            continue;
        }

        // 检查是否超时
        LARGE_INTEGER current;
        QueryPerformanceCounter(&current);
        double elapsed_ms = (current.QuadPart - start_wait.QuadPart) * 1000.0 / frequency.QuadPart;
        if (elapsed_ms > timeout_ms) {
            setLastError("读取超时");
            return false;
        }

        // // 短暂休眠，避免过度占用CPU
        // Sleep(1);
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
    if(read_count % 10000 == 0 || read_count < 1000) {
        logMsg.str("");
        logMsg << "第 " << read_count << " 次读取的数: ";
        for(size_t i = 0; i < last_package_size/sizeof(unsigned int); i++) {
            logMsg << "0x" << std::hex << std::setfill('0') << std::setw(8) 
                  << read_buffer[i] << " ";
        }
        handleLog(logMsg.str());
    }

    // 更新读取位置
    next_read_pos += last_package_size;

    // 更新包计数器
    package_counter--;

    // 每10000次读取打印一次读取操作信息
    if(read_count % 10000 == 0 || read_count < 1000) {
        logMsg.str("");
        logMsg << "从FIFO位置 " << (next_read_pos - last_package_size)
               << " 读取了 " << last_package_size
               << " 字节数据，剩余未读组数: " << package_counter;
        handleLog(logMsg.str());
    }

    return true;
}

Pice_dll::VersionInfo Pice_dll::getVersionInfo()
{
    VersionInfo info;
    std::stringstream logMsg;
    
    // 直接设置默认版本
    info.hardwareVersion = "暂无";
    info.softwareVersion = "v0.1";

    // 记录日志
    logMsg << "获取版本信息 - 硬件版本: " << info.hardwareVersion 
           << ", 软件版本: " << info.softwareVersion;
    handleLog(logMsg.str());

    return info;
}

// void Pice_dll::setFifoEnabled(bool enabled) {
//     if (!enabled && fifo_thread) {
//         fifo_thread->stop();  // 停止FIFO线程
//         fifo_thread->wait();
//         delete fifo_thread;
//         fifo_thread = nullptr;
//     }
// }

// 导出函数实现
#ifdef PICE_DLL_LIB
extern "C" {
    __declspec(dllexport) void* CreatePiceDll() {
        return new Pice_dll();
    }

    __declspec(dllexport) void DestroyPiceDll(void* instance) {
        delete static_cast<Pice_dll*>(instance);
    }

    __declspec(dllexport) int CheckPcie(void* instance) {
        return static_cast<Pice_dll*>(instance)->checkPcie();
    }

    __declspec(dllexport) int OpenPcie(void* instance) {
        return static_cast<Pice_dll*>(instance)->openPcie();
    }

    __declspec(dllexport) void ClosePcie(void* instance) {
        static_cast<Pice_dll*>(instance)->closePcie();
    }

    __declspec(dllexport) bool FpgaFifoStart(void* instance, int value) {
        return static_cast<Pice_dll*>(instance)->fpga_fifo_start(value);
    }

    __declspec(dllexport) void FpgaFifoOnce(void* instance) {
        static_cast<Pice_dll*>(instance)->fpga_fifo_once();
    }

    __declspec(dllexport) bool FpgaRead(void* instance, unsigned int* buffer, int timeout_ms) {
        return static_cast<Pice_dll*>(instance)->fpga_read(buffer, timeout_ms);
    }

    __declspec(dllexport) const char* GetPiceDllError(void* instance) {
        static std::string error = static_cast<Pice_dll*>(instance)->getLastError();
        return error.c_str();
    }

    __declspec(dllexport) bool IsConnected(void* instance) {
        return static_cast<Pice_dll*>(instance)->isConnected();
    }

    __declspec(dllexport) size_t GetCurrentFifoPos(void* instance) {
        return static_cast<Pice_dll*>(instance)->getCurrentFifoPos();
    }

    __declspec(dllexport) size_t GetNextReadPos(void* instance) {
        return static_cast<Pice_dll*>(instance)->getNextReadPos();
    }

    __declspec(dllexport) void EnableRiffaLog(bool enable) {
        Pice_dll::enableRiffaLog(enable);
    }

    __declspec(dllexport) void EnablePiceDllLog(bool enable) {
        Pice_dll::enablePiceDllLog(enable);
    }

    __declspec(dllexport) void SetLogCallback(void* instance, LogCallback callback) {
        static_cast<Pice_dll*>(instance)->setLogCallback(callback);
    }

    __declspec(dllexport) void WaitMicroseconds(void* instance, int microseconds) {
        static_cast<Pice_dll*>(instance)->waitMicroseconds(microseconds);
    }

    __declspec(dllexport) int64_t GetCurrentTimeMicros(void* instance) {
        return static_cast<Pice_dll*>(instance)->getCurrentTimeMicros();
    }

    __declspec(dllexport) void ResetTimer(void* instance) {
        static_cast<Pice_dll*>(instance)->resetTimer();
    }

    __declspec(dllexport) int64_t GetElapsedMicros(void* instance) {
        return static_cast<Pice_dll*>(instance)->getElapsedMicros();
    }

    __declspec(dllexport) void GetVersionInfo(void* instance, char* hwVer, char* swVer, int bufSize) {
        Pice_dll* pice = static_cast<Pice_dll*>(instance);
        Pice_dll::VersionInfo info = pice->getVersionInfo();
        
        strncpy_s(hwVer, bufSize, info.hardwareVersion.c_str(), _TRUNCATE);
        strncpy_s(swVer, bufSize, info.softwareVersion.c_str(), _TRUNCATE);
    }
}
#endif

// 实现 waitMicroseconds 函数
void Pice_dll::waitMicroseconds(int microseconds)
{
    LARGE_INTEGER start, current;
    QueryPerformanceCounter(&start);
    
    double target_time = (microseconds * frequency.QuadPart) / 1000000.0;
    
    do {
        QueryPerformanceCounter(&current);
        double elapsed = current.QuadPart - start.QuadPart;
        if (elapsed >= target_time) break;
        
        Sleep(0);
    } while (true);
}

// 静态成员函数实现
void Pice_dll::enableRiffaLog(bool enable) { 
    riffa_log_enabled = enable; 
}

bool Pice_dll::isRiffaLogEnabled() { 
    return riffa_log_enabled; 
}

void Pice_dll::enablePiceDllLog(bool enable) { 
    pice_dll_log_enabled = enable; 
}

bool Pice_dll::isPiceDllLogEnabled() { 
    return pice_dll_log_enabled; 
}

void Pice_dll::setLogCallback(LogCallback callback) { 
    log_callback = callback; 
}

LogCallback Pice_dll::getLogCallback() { 
    return log_callback; 
}

Pice_dll* Pice_dll::getInstance() { 
    return instance; 
}

void Pice_dll::setInstance(Pice_dll* inst) { 
    instance = inst; 
}
