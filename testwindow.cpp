#include "testwindow.h"
#include "ui_testwindow.h"
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>

// 初始化静态成员变量
int TestWindow::currentLoopCount = 0;

TestWindow::TestWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TestWindow),
    pcie(new Pice_dll()),
    receiveBuffer(nullptr),
    logFile(nullptr),
    logStream(nullptr),
    fifoReaderThread(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("PCIE DLL 测试程序");
    
    // 连接日志信号
    connect(pcie, &Pice_dll::logGenerated, this, &TestWindow::onLogGenerated);
    
    // 初始化日志文件
    initLogFile();
}

TestWindow::~TestWindow()
{
    if(fifoReaderThread) {
        fifoReaderThread->stop();
        fifoReaderThread->wait();
        delete fifoReaderThread;
    }
    closeLogFile();
    delete ui;
    if(receiveBuffer) {
        delete[] receiveBuffer;
    }
    delete pcie;
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
    ui->textLog->append(text);
    writeToLogFile(text);
}

void TestWindow::on_btnCheckPcie_clicked()
{
    appendLog("正在检查PCIE设备...");
    int devices = pcie->checkPcie();
    appendLog(QString("找到 %1 个设备").arg(devices));
}

void TestWindow::on_btnOpenPcie_clicked()
{    
    appendLog("正在打开PCIE设备...");
    if(pcie->openPcie() == 0) {
        appendLog("PCIE设备打开成功");
        ui->btnStartFifo->setEnabled(true);
    } else {
        appendLog("打开PCIE设备失败: " + pcie->getLastError());
    }
}

void TestWindow::on_btnClosePcie_clicked()
{
    appendLog("正在关闭PCIE设备...");
    
    if(fifoReaderThread) {
        fifoReaderThread->stop();
        fifoReaderThread->wait();
        delete fifoReaderThread;
        fifoReaderThread = nullptr;
    }
    
    pcie->closePcie();
    ui->btnStartFifo->setEnabled(false);
    appendLog("PCIE设备已关闭");
}

void TestWindow::onLogGenerated(const QString& message)
{
    writeToLogFile("RIFFA: " + message);
}

void TestWindow::onWorkerLogMessage(const QString& message)
{
    appendLog(message);
}

// FIFO读取线程实现
FifoReaderThread::FifoReaderThread(Pice_dll* pcie, QObject* parent)
    : QThread(parent), m_pcie(pcie), m_running(true)
{
}

void FifoReaderThread::stop()
{
    m_running = false;
}

void FifoReaderThread::run()
{
    unsigned int* readBuffer = new unsigned int[BUFFER_SIZE];
    qint64 readCount = 0;
    QElapsedTimer timer;
    timer.start();
    
    while(m_running) {
        // 每10微秒执行一次读取
        if(timer.nsecsElapsed() >= 10000) {  // 10000纳秒 = 10微秒
            timer.restart();  // 重置计时器
            
            // 读取数据
            if(m_pcie->fpga_read(readBuffer)) {
                readCount++;
                
                // 每10000次打印一次状态
                if(readCount % 10000 == 0) {
                    emit readCompleted(readCount, timer.nsecsElapsed() / 1000);  // 转换为微秒
                    emit logMessage(QString("已完成 %1 次读取，当前位置: %2 MB，FIFO写入位置: %3 MB")
                        .arg(readCount)
                        .arg(m_pcie->getNextReadPos() / (1024 * 1024))
                        .arg(m_pcie->getCurrentFifoPos() / (1024 * 1024)));
                }
            } else {
                if(m_running) {  // 只在非停止状态下输出错误
                    // 如果是超时错误，不输出日志，继续尝试
                    if(m_pcie->getLastError() != "读取超时") {
                        emit logMessage("读取失败: " + m_pcie->getLastError());
                    }
                }
            }
        }
        
        // 等待10微秒
        QThread::usleep(10);  // 10微秒延时
    }
    
    delete[] readBuffer;
    emit logMessage("FIFO读取线程结束");
}

void TestWindow::on_btnStartFifo_clicked()
{
    bool ok;
    int value = ui->lineEditValue->text().toInt(&ok);
    if(!ok) {
        QMessageBox::warning(this, "错误", "请输入有效的数值");
        return;
    }
    
    // 启动FIFO写入
    appendLog(QString("开始FIFO操作，输入值: %1").arg(value));
    if(pcie->fpga_fifo(value)) {
        appendLog("FIFO写入线程已启动");
        
        // 创建并启动读取线程
        if(fifoReaderThread) {
            fifoReaderThread->stop();
            fifoReaderThread->wait();
            delete fifoReaderThread;
        }
        
        fifoReaderThread = new FifoReaderThread(pcie, this);
        connect(fifoReaderThread, &FifoReaderThread::logMessage, 
                this, &TestWindow::onWorkerLogMessage);
        connect(fifoReaderThread, &FifoReaderThread::readCompleted,
                this, &TestWindow::onFifoReadCompleted);
        
        fifoReaderThread->start();
    } else {
        appendLog("FIFO操作启动失败: " + pcie->getLastError());
    }
}

void TestWindow::onFifoReadCompleted(qint64 count, qint64 time)
{
    appendLog(QString("第 %1 次FIFO读取完成，耗时: %2 微秒")
        .arg(count)
        .arg(time));
} 