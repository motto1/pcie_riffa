#ifndef TESTWINDOW_H
#define TESTWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <QElapsedTimer>
#include "pice_dll.h"

// 声明 DLL 导出函数
extern "C" {
    __declspec(dllimport) bool FpgaRead(void* instance, unsigned int* buffer, int timeout_ms);
    __declspec(dllimport) int64_t GetElapsedMicros(void* instance);
    __declspec(dllimport) void* CreatePiceDll();
    __declspec(dllimport) void DestroyPiceDll(void* instance);
    __declspec(dllimport) int CheckPcie(void* instance);
    __declspec(dllimport) int OpenPcie(void* instance);
    __declspec(dllimport) void ClosePcie(void* instance);
    __declspec(dllimport) bool FpgaFifoStart(void* instance, int value);
    __declspec(dllimport) void FpgaFifoOnce(void* instance);
    __declspec(dllimport) const char* GetPiceDllError(void* instance);
    __declspec(dllimport) bool IsConnected(void* instance);
    __declspec(dllimport) void EnableRiffaLog(bool enable);
    __declspec(dllimport) void EnablePiceDllLog(bool enable);
    __declspec(dllimport) void SetLogCallback(void* instance, LogCallback callback);
    __declspec(dllimport) void GetVersionInfo(void* instance, char* hwVer, char* swVer, int bufSize);
}

// FIFO读取线程类
class FifoReaderThread : public QThread
{
    Q_OBJECT
public:
    explicit FifoReaderThread(void* pcie_instance, QObject* parent = nullptr)
        : QThread(parent), m_pcie_instance(pcie_instance), m_running(false) {
        m_buffer = new unsigned int[BUFFER_SIZE];
    }
    
    ~FifoReaderThread() {
        delete[] m_buffer;
    }
    
    void stop() { m_running = false; }

signals:
    void logMessage(const QString& message);
    void readCompleted(qint64 count, qint64 time);

protected:
    void run() override {
        m_running = true;
        qint64 readCount = 0;
        QElapsedTimer timer;
        timer.start();
        
        while(m_running) {
            // 检查连接状态
            if (!IsConnected(m_pcie_instance)) {
                emit logMessage("设备未连接，停止FIFO读取线程");
                break;
            }

            // 读取数据
            if(FpgaRead(m_pcie_instance, m_buffer, 10000)) {
                readCount++;
                
                // 每10000次打印一次状态
                if(readCount % 10000 == 0 || readCount < 1000) {
                    emit readCompleted(readCount, timer.nsecsElapsed() / 1000);  // 转换为微秒
                    QString logMsg = QString("已完成 %1 次读取").arg(readCount);
                    emit logMessage(logMsg);
                    timer.restart();
                }
            } else {
                if(m_running) {
                    // 如果是超时错误，不输出日志，继续尝试
                    const char* error = GetPiceDllError(m_pcie_instance);
                    if(strcmp(error, "读取超时") != 0) {
                        emit logMessage(QString("读取失败: %1").arg(error));
                    }
                }
            }
        }
        
        emit logMessage("FIFO读取线程结束");
    }

private:
    void* m_pcie_instance;
    bool m_running;
    unsigned int* m_buffer;
    static const int BUFFER_SIZE = 4096 * 4;
};

namespace Ui {
class TestWindow;
}

class TestWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TestWindow(QWidget *parent = nullptr);
    ~TestWindow();
    static int currentLoopCount;

public slots:
    void appendLog(const QString& text);

private slots:
    void on_btnCheckPcie_clicked();
    void on_btnOpenPcie_clicked();
    void on_btnClosePcie_clicked();
    void on_btnStartFifo_clicked();
    void on_btnToggleFifo_clicked();
    void onFifoReadCompleted(qint64 count, qint64 time);
    void onWorkerLogMessage(const QString& message);

private:
    Ui::TestWindow *ui;
    void* pcie_instance;  // 改用 void* 存储 DLL 实例
    QFile* logFile;
    QTextStream* logStream;
    FifoReaderThread* fifoReaderThread;
    
    void writeToLogFile(const QString& text);
    void initLogFile();
    void closeLogFile();
};

#endif // TESTWINDOW_H 
