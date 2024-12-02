#ifndef PICE_DLL_H
#define PICE_DLL_H

#include <string>
#include <windows.h>
#include "riffa.h"

#ifdef PICE_DLL_LIB
    #define PICE_DLL_EXPORT __declspec(dllexport)
#else
    #define PICE_DLL_EXPORT __declspec(dllimport)
#endif

// 日志回调函数类型定义
typedef void (*LogCallback)(const char* message);

class PICE_DLL_EXPORT Pice_dll {
public:
    Pice_dll();
    ~Pice_dll();

    // FPGA操作相关函数
    int checkPcie();  
    int openPcie();   
    void closePcie(); 
    
    // 状态查询
    const char* getLastError() const { return lastError.c_str(); }
    bool isConnected() const { return connected; }
    
    // FIFO操作
    bool fpga_fifo_start(int value);// 启动FIFO
    void fpga_fifo_once();// FIFO读取一次
    size_t getCurrentFifoPos() const { return current_buffer_pos; }// 获取当前FIFO位置
    size_t getNextReadPos() const { return next_read_pos; }// 获取下一个读取位置
    bool fpga_read(unsigned int* read_buffer, int timeout_ms = 1000);// 读取FIFO数据

    // 日志控制
    static void enableRiffaLog(bool enable);
    static bool isRiffaLogEnabled();
    static void enablePiceDllLog(bool enable);
    static bool isPiceDllLogEnabled();
    static void setLogCallback(LogCallback callback);
    static LogCallback getLogCallback();

    // 版本信息相关函数
    struct VersionInfo {
        std::string hardwareVersion;
        std::string softwareVersion;
    };
    VersionInfo getVersionInfo();

    // 实例管理
    static Pice_dll* getInstance();
    static void setInstance(Pice_dll* inst);
    void handleLog(const std::string& message);  // 移到public

    // 添加计时相关函数声明
    int64_t getCurrentTimeMicros();
    void resetTimer();
    int64_t getElapsedMicros();
    void waitMicroseconds(int microseconds);

private:
    // 静态成员声明
    static Pice_dll* instance;
    static bool riffa_log_enabled;
    static bool pice_dll_log_enabled;
    static LogCallback log_callback;

    // FPGA相关变量
    fpga_t* fpga;
    fpga_info_list fpga_pcie_info;
    int fpga_pcie_id;
    int fpga_pcie_chnl;
    unsigned int* pcie_Send_Buffer;
    int input_value;
    std::string lastError;
    bool connected;
    bool prepared;
    int sent;
    
    // 计时相关
    LARGE_INTEGER frequency;
    LARGE_INTEGER start_time;
    LARGE_INTEGER last_time;

    // FIFO缓冲区
    unsigned char* fifo_buffer;
    size_t buffer_size;
    size_t current_buffer_pos;
    static const size_t FIFO_BUFFER_SIZE = 500 * 1024 * 1024;  // 500MB
    int count;

    // 读取相关变量
    size_t next_read_pos;
    size_t last_package_size;
    bool has_new_data;
    int package_counter;
    static const int MAX_PACKAGE_COUNT = 5000;
    int read_count;

    // 辅助函数
    void setLastError(const std::string& error);
    void cleanup();
    void logMessage(const std::string& message);
};

// 导出函数声明
#ifdef PICE_DLL_LIB
extern "C" {
    __declspec(dllexport) void* CreatePiceDll();
    __declspec(dllexport) void DestroyPiceDll(void* instance);
    __declspec(dllexport) int CheckPcie(void* instance);
    __declspec(dllexport) int OpenPcie(void* instance);
    __declspec(dllexport) void ClosePcie(void* instance);
    __declspec(dllexport) bool FpgaFifoStart(void* instance, int value);
    __declspec(dllexport) void FpgaFifoOnce(void* instance);
    __declspec(dllexport) bool FpgaRead(void* instance, unsigned int* buffer, int timeout_ms);
    __declspec(dllexport) const char* GetPiceDllError(void* instance);
    __declspec(dllexport) bool IsConnected(void* instance);
    __declspec(dllexport) size_t GetCurrentFifoPos(void* instance);
    __declspec(dllexport) size_t GetNextReadPos(void* instance);
    __declspec(dllexport) void EnableRiffaLog(bool enable);
    __declspec(dllexport) void EnablePiceDllLog(bool enable);
    __declspec(dllexport) void SetLogCallback(void* instance, LogCallback callback);
    __declspec(dllexport) void WaitMicroseconds(void* instance, int microseconds);
    __declspec(dllexport) int64_t GetCurrentTimeMicros(void* instance);
    __declspec(dllexport) void ResetTimer(void* instance);
    __declspec(dllexport) int64_t GetElapsedMicros(void* instance);
    __declspec(dllexport) void GetVersionInfo(void* instance, char* hwVer, char* swVer, int bufSize);
}
#else
extern "C" {
    __declspec(dllimport) void* CreatePiceDll();
    __declspec(dllimport) void DestroyPiceDll(void* instance);
    // ... 其他导入函数声明 ...
    __declspec(dllimport) void GetVersionInfo(void* instance, char* hwVer, char* swVer, int bufSize);
}
#endif

#endif // PICE_DLL_H
