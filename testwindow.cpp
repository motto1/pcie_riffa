#include "testwindow.h"
#include "ui_testwindow.h"
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>

// 初始化静态成员变量
int TestWindow::currentLoopCount = 0;

// 定义静态回调函数
static void LogCallbackFunc(const char* message) {
    // 获取当前窗口实例
    if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow())) {
        if (TestWindow* testWindow = qobject_cast<TestWindow*>(mainWindow)) {
            // 在主线程中处理日志
            QMetaObject::invokeMethod(testWindow, "appendLog", 
                Qt::QueuedConnection,
                Q_ARG(QString, QString::fromUtf8(message)));
        }
    }
}

TestWindow::TestWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TestWindow)
    , pcie_instance(nullptr)
    , logFile(nullptr)
    , logStream(nullptr)
    , fifoReaderThread(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("PCIE DLL 测试程序");
    
    // 创建 DLL 实例
    pcie_instance = CreatePiceDll();
    
    // 初始化日志文件
    initLogFile();
    
    // 设置日志回调
    SetLogCallback(pcie_instance, LogCallbackFunc);
    
    // 连接日志控制复选框
    connect(ui->checkBoxRiffaLog, &QCheckBox::toggled, this, [this](bool checked) {
        EnableRiffaLog(checked);
    });
    
    connect(ui->checkBoxPiceDllLog, &QCheckBox::toggled, this, [this](bool checked) {
        EnablePiceDllLog(checked);
    });

    // 连接新的日志控制复选框
    connect(ui->checkBoxLogToFile, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked && logFile) {
            // 如果禁用日志文件，关闭当前日志文件
            closeLogFile();
        } else if (checked && !logFile) {
            // 如果启用日志文件，重新创建日志文件
            initLogFile();
        }
    });
}

TestWindow::~TestWindow()
{
    if (fifoReaderThread) {
        fifoReaderThread->stop();
        fifoReaderThread->wait();
        delete fifoReaderThread;
    }
    
    if (pcie_instance) {
        ClosePcie(pcie_instance);
        DestroyPiceDll(pcie_instance);
    }
    
    closeLogFile();
    delete ui;
}

void TestWindow::initLogFile()
{
    // 创建logs目录
    QDir dir;
    if (!dir.exists("logs")) {
        dir.mkdir("logs");
    }
    
    // 创建日志文件，使用时间戳作为文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    logFile = new QFile(QString("logs/pcie_test_%1.log").arg(timestamp));
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        logStream = new QTextStream(logFile);
        *logStream << "PCIE测试程序日志 - " << QDateTime::currentDateTime().toString() << "\n";
        logStream->flush();
    } else {
        qDebug() << "无法创建日志文件";
        delete logFile;
        logFile = nullptr;
    }
}

void TestWindow::closeLogFile()
{
    if (logStream) {
        delete logStream;
        logStream = nullptr;
    }
    if (logFile) {
        if (logFile->isOpen()) {
            logFile->close();
        }
        delete logFile;
        logFile = nullptr;
    }
}

void TestWindow::writeToLogFile(const QString& text)
{
    if (logStream) {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        *logStream << QString("[%1] %2\n").arg(timestamp).arg(text);
        logStream->flush();
    }
}

void TestWindow::appendLog(const QString& text)
{
    // 根据复选框状态决定是否写入文本框
    if (ui->checkBoxLogToTextBox->isChecked()) {
        ui->textLog->append(text);
    }
    
    // 根据复选框状态决定是否写入日志文件
    if (ui->checkBoxLogToFile->isChecked()) {
        writeToLogFile(text);
    }
}
void TestWindow::on_btnCheckPcie_clicked()
{
    // 获取版本信息
    char hwVer[256] = {0};
    char swVer[256] = {0};
    GetVersionInfo(pcie_instance, hwVer, swVer, sizeof(hwVer));
    
    appendLog(QString("硬件版本: %1").arg(hwVer));
    appendLog(QString("软件版本: %1").arg(swVer));
    
    // 检查PCIE设备
    appendLog("正在检查PCIE设备...");
    int devices = CheckPcie(pcie_instance);
    if (devices > 0) {
        appendLog(QString("找到 %1 个设备").arg(devices));
        ui->btnOpenPcie->setEnabled(true);
    } else {
        appendLog("未找到PCIE设备");
        ui->btnOpenPcie->setEnabled(false);
    }
}

void TestWindow::on_btnOpenPcie_clicked()
{    
    appendLog("正在打开PCIE设备...");
    if (OpenPcie(pcie_instance) == 0) {
        appendLog("PCIE设备打开成功");
        ui->btnStartFifo->setEnabled(true);
        ui->btnOpenPcie->setEnabled(false);
        ui->btnClosePcie->setEnabled(true);
    } else {
        appendLog(QString("打开PCIE设备失败: %1").arg(GetPiceDllError(pcie_instance)));
    }
}

void TestWindow::on_btnClosePcie_clicked()
{
    appendLog("正在关闭PCIE设备...");
    ClosePcie(pcie_instance);
    ui->btnStartFifo->setEnabled(false);
    ui->btnOpenPcie->setEnabled(true);
    ui->btnClosePcie->setEnabled(false);
    appendLog("PCIE设备已关闭");
}



void TestWindow::onWorkerLogMessage(const QString& message)
{
    appendLog(message);
}

void TestWindow::on_btnStartFifo_clicked()
{
    bool ok;
    int value = ui->lineEditValue->text().toInt(&ok);
    if (!ok) {
        appendLog("请输入有效的数值");
        return;
    }
    // 锁定其他按钮
    ui->btnStartFifo->setEnabled(false);
    ui->btnToggleFifo->setEnabled(true); // 关闭线程的按钮保持可用
    ui->btnCheckPcie->setEnabled(false);
    ui->btnOpenPcie->setEnabled(false);
    ui->btnClosePcie->setEnabled(false);
    // 启动FIFO操作
    appendLog(QString("开始FIFO操作，输入值: %1").arg(value));
    if (FpgaFifoStart(pcie_instance, value)) {
        appendLog("FIFO操作启动成功");
        
        // 创建并启动读取线程
        if (!fifoReaderThread) {
            fifoReaderThread = new FifoReaderThread(pcie_instance, this);
            connect(fifoReaderThread, &FifoReaderThread::logMessage, 
                    this, &TestWindow::onWorkerLogMessage);
            connect(fifoReaderThread, &FifoReaderThread::readCompleted,
                    this, &TestWindow::onFifoReadCompleted);
            fifoReaderThread->start();
        }

        // 启动FIFO写入线程
        QThread* fifoWriteThread = new QThread;
        QObject* worker = new QObject;
        worker->moveToThread(fifoWriteThread);
        
        connect(fifoWriteThread, &QThread::started, [this, worker]() {
            while (IsConnected(pcie_instance) && fifoReaderThread && !fifoReaderThread->isFinished()) {
                FpgaFifoOnce(pcie_instance);
            }
            worker->deleteLater();
        });
        
        connect(worker, &QObject::destroyed, fifoWriteThread, &QThread::quit);
        connect(fifoWriteThread, &QThread::finished, fifoWriteThread, &QThread::deleteLater);
        
        fifoWriteThread->start();
        appendLog("FIFO写入线程已启动");
    } else {
        appendLog(QString("FIFO操作启动失败: %1").arg(GetPiceDllError(pcie_instance)));
        // 如果启动失败，停止读取线程
        if (fifoReaderThread) {
            fifoReaderThread->stop();
            fifoReaderThread->wait();
            delete fifoReaderThread;
            fifoReaderThread = nullptr;
        }
    }
}

void TestWindow::onFifoReadCompleted(qint64 count, qint64 time)
{
    appendLog(QString("第 %1 次FIFO读取完成，耗时: %2 微秒")
        .arg(count)
        .arg(time));
}

void TestWindow::on_btnToggleFifo_clicked()
{
    // 停止FIFO读取线程
    if (fifoReaderThread) {
        fifoReaderThread->stop();
        fifoReaderThread->wait();
        delete fifoReaderThread;
        fifoReaderThread = nullptr;
        appendLog("已停止FIFO读取线程");
    }

    // 停止所有FIFO写入线程
    for (QThread* thread : this->findChildren<QThread*>()) {
        if (thread->objectName().contains("fifoWriteThread")) {
            thread->quit();
            thread->wait();
            thread->deleteLater();
        }
    }
    appendLog("已停止FIFO写入线程");

    // 启用相关按钮
    ui->btnStartFifo->setEnabled(true);
    ui->btnToggleFifo->setEnabled(true);
    ui->btnCheckPcie->setEnabled(true);
    ui->btnOpenPcie->setEnabled(true);
    ui->btnClosePcie->setEnabled(true);
    appendLog("已重新启用所有操作按钮");
}

