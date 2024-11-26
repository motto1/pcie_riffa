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

// 添加工作线程类
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(Pice_dll* pcie, unsigned int* buffer, QObject* parent = nullptr);
    void stop();

signals:
    void logMessage(const QString& message);
    void operationCompleted(int count, qint64 time);

protected:
    void run() override;

private:
    Pice_dll* m_pcie;
    unsigned int* m_buffer;
    bool m_running;
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

private slots:
    void on_btnCheckPcie_clicked();
    void on_btnOpenPcie_clicked();
    void on_btnPrepareSend_clicked();
    void on_btnSendRecv_clicked();
    void on_btnClosePcie_clicked();
    void onLogGenerated(const QString& message);
    void onWorkerLogMessage(const QString& message);
    void onOperationCompleted(int count, qint64 time);

private:
    Ui::TestWindow *ui;
    Pice_dll* pcie;
    unsigned int* receiveBuffer;
    QFile* logFile;
    QTextStream* logStream;
    WorkerThread* workerThread;
    
    void appendLog(const QString& text);
    void writeToLogFile(const QString& text);
    void initLogFile();
    void closeLogFile();
};

#endif // TESTWINDOW_H 