#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QString>
#include <QDir>
#include <QTimer>
#include "riffa.h"
#include <sstream>
#include <iostream>
#include <memory>
#include <functional>
#include <QThread>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QApplication>
#include <QElapsedTimer>

// 前向声明
namespace Ui {
    class MainWindow;
}

class MainWindow;  // 前向声明MainWindow类

// 自定义输出缓冲区类
class CustomBuffer : public std::streambuf {
public:
    CustomBuffer(std::function<void(const QString&)> callback) : m_callback(callback) {}

protected:
    virtual int_type overflow(int_type c) {
        if (c != EOF) {
            m_buffer += QChar(c);
            if (c == '\n') {
                m_callback(m_buffer);
                m_buffer.clear();
            }
        }
        return c;
    }

    virtual int sync() {
        if (!m_buffer.isEmpty()) {
            m_callback(m_buffer);
            m_buffer.clear();
        }
        return 0;
    }

private:
    QString m_buffer;
    std::function<void(const QString&)> m_callback;
};

// FPGA线程类
class FPGAThread : public QThread {
    Q_OBJECT
public:
    explicit FPGAThread(MainWindow* mainWindow, fpga_t* fpga, unsigned int* send_buf, 
                       unsigned int* recv_buf, int sent_val, int input_val, 
                       long num, QObject* parent = nullptr);
    void stopThread();

protected:
    void run() override;

signals:
    void updateDisplay(const QString& text);
    void clearDisplay();
    void operationComplete();

private:
    MainWindow* m_mainWindow;  // 指向MainWindow的指针
    fpga_t* m_fpga;
    unsigned int* m_sendBuffer;
    unsigned int* m_recvBuffer;
    int m_sent;
    long m_num;
    int m_inputValue;
    bool m_isRunning;
    QMutex m_mutex;
    QString m_logFilePath;  // 日志文件路径
    QFile m_logFile;       // 日志文件对象
    QTextStream m_logStream; // 日志文本流
    
    // 添加写日志的辅助方法
    void writeLog(const QString& message) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        QString logMessage = QString("[%1] %2").arg(timestamp).arg(message);
        
        if (m_logFile.isOpen()) {
            m_logStream << logMessage << Qt::endl;
            m_logStream.flush();  // 确保立即写入
        }
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    //fpga_pcie变量部分
    fpga_t * fpga;
    fpga_info_list fpga_pcie_info;
    int fpga_pcie_id;
    int fpga_pcie_chnl;
    unsigned int * pcie_Send_Buffer;
    unsigned int * pcie_recv_Buffer;
    unsigned int * old_recv_Buffer;
    int openpcie();  // 移到public
    void closepcie(); // 移到public
    int input_value;// 输入值
    int sent;
    long num;
    void executeSingleOperation();  // 添加这个函数声明
    qint64 old_elapsedMicros;

private slots:
    int checkpcie();
    void on_Btn_Check_clicked();
    void on_btn_open_pcie_clicked();
    void on_Btn_close_pcie_clicked();
    void on_Btn_AD_Connect_Start_clicked();
    void on_Btn_Correct_clicked();
    void on_Btn_Stop_clicked();
    void on_clean_textBrowser_clicked();
    void on_Btn_AD_Connect_Start_timer_clicked();
    void on_timer_stop_clicked();
    

private:
    FPGAThread* m_fpgaThread;  // 添加线程指针成员
    void appendToTextBrowser4(const QString& text);
    Ui::MainWindow *ui;
    std::unique_ptr<CustomBuffer> m_buffer;
    std::streambuf* m_oldCoutBuf;
    std::streambuf* m_oldCerrBuf;
    QTimer* timer;  // 添加timer成员变量
    
    // 添加日志相关成员
    QFile m_logFile;
    QTextStream m_logStream;
    int m_lineCount;  // 添加行数计数器
    
    // 添加日志写入函数
    void writeLog(const QString& message);
    void updateDisplay(const QString& message);
    QElapsedTimer m_loopTimer;  // 添加计时器
    qint64 m_lastElapsedMicros; // 上一次的时间戳
};

#endif // MAINWINDOW_H
