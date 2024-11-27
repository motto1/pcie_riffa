#ifndef TESTWINDOW_H
#define TESTWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include "pice_dll.h"

// 添加FIFO读取线程类
class FifoReaderThread : public QThread
{
    Q_OBJECT
public:
    FifoReaderThread(Pice_dll* pcie, QObject* parent = nullptr);
    void stop();

signals:
    void logMessage(const QString& message);
    void readCompleted(qint64 count, qint64 time);

protected:
    void run() override;

private:
    Pice_dll* m_pcie;
    bool m_running;
    static const int BUFFER_SIZE = 4096 * 4;  // 接收缓冲区大小
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

private slots:
    void on_btnCheckPcie_clicked();
    void on_btnOpenPcie_clicked();
    void on_btnClosePcie_clicked();
    void onLogGenerated(const QString& message);
    void onWorkerLogMessage(const QString& message);
    void on_btnStartFifo_clicked();    // FIFO操作按钮
    void onFifoReadCompleted(qint64 count, qint64 time);

private:
    Ui::TestWindow *ui;
    Pice_dll* pcie;
    unsigned int* receiveBuffer;
    QFile* logFile;
    QTextStream* logStream;
    FifoReaderThread* fifoReaderThread;  // 只保留FIFO读取线程
    
    void appendLog(const QString& text);
    void writeToLogFile(const QString& text);
    void initLogFile();
    void closeLogFile();
};

#endif // TESTWINDOW_H 