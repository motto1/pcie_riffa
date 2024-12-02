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
    Pice_dll::VersionInfo version = pcie->getVersionInfo();
    appendLog(QString("硬件版本: %1").arg(version.hardwareVersion));
    appendLog(QString("软件版本: %1").arg(version.softwareVersion));
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
    pcie->closePcie();
    ui->btnStartFifo->setEnabled(false);
    appendLog("PCIE设备已关闭");
}

void TestWindow::onLogGenerated(const QString& message)
{
    // 日志已经在handleLog中添加了前缀，直接写入和显示
    writeToLogFile(message);
    ui->textLog->append(message);
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
        // 检查连接状态，如果未连接则停止线程
        if (!m_pcie->isConnected()) {
            emit logMessage("设备未连接，停止FIFO读取线程");
            break; // 退出循环
        }

        // 读取数据，设置较短的超时时间（100微秒）
        if(m_pcie->fpga_read(readBuffer, 10000)) {
            readCount++;
            
            // 每10000次打印一次状态
            if(readCount % 10000 == 0 || readCount < 1000) {
                emit readCompleted(readCount, timer.nsecsElapsed() / 1000);  // 转换为微秒
                QString logMsg = QString("已完成 %1 次读取，当前位置: %2 MB，FIFO写入位置: %3 MB")
                    .arg(readCount)
                    .arg(m_pcie->getNextReadPos() / (1024 * 1024))
                    .arg(m_pcie->getCurrentFifoPos() / (1024 * 1024));
                emit logMessage(logMsg);
                qDebug() << logMsg; // 添加控制台输出
                timer.restart();  // 重置计时器
            }
        } else {
            if(m_running) {  // 只在非停止状态下输出错误
                // 如果是超时错误，不输出日志，继续尝试
                if(m_pcie->getLastError() != "读取超时") {
                    emit logMessage("读取失败: " + m_pcie->getLastError());
                }
            }
        }
        // // 精确控制10微秒的延时
        // QThread::usleep(10);  // 10微秒延时
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
    
    // 根据复选框状态设置日志
    Pice_dll::enableRiffaLog(ui->checkBoxRiffaLog->isChecked());
    Pice_dll::enablePiceDllLog(ui->checkBoxPiceDllLog->isChecked());
    
    // 锁定其他按钮
    ui->btnStartFifo->setEnabled(false);
    ui->btnToggleFifo->setEnabled(true); // 关闭线程的按钮保持可用
    ui->btnCheckPcie->setEnabled(false);
    ui->btnOpenPcie->setEnabled(false);
    ui->btnClosePcie->setEnabled(false);

    // 先创建并启动读取线程
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
    
    // 启动读取线程
    fifoReaderThread->start();
    
    // 然后启动FIFO写入
    appendLog(QString("开始FIFO操作，输入值: %1").arg(value));
    if(pcie->fpga_fifo_start(value)) {
        appendLog("FIFO写入初始化成功");
        
        // 创建一个新线程来循环执行FIFO写入操作
        QThread* fifoWriteThread = new QThread;
        QObject* worker = new QObject;
        worker->moveToThread(fifoWriteThread);
        
        connect(fifoWriteThread, &QThread::started, [this, worker]() {
            while(!fifoReaderThread->isFinished()) {
                pcie->fpga_fifo_once();
            }
            worker->deleteLater();
        });
        
        connect(worker, &QObject::destroyed, fifoWriteThread, &QThread::quit);
        connect(fifoWriteThread, &QThread::finished, fifoWriteThread, &QThread::deleteLater);
        
        fifoWriteThread->start();
        appendLog("FIFO写入线程已启动");
    } else {
        appendLog("FIFO操作启动失败: " + pcie->getLastError());
        // 如果启动失败，停止读取线程
        if(fifoReaderThread) {
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
    if(fifoReaderThread) {
        fifoReaderThread->stop();
        fifoReaderThread->wait();
        delete fifoReaderThread;
        fifoReaderThread = nullptr;
    }
    // pcie->setFifoEnabled(false);
    // ui->btnToggleFifo->setText("关闭FIFO线程与读取线程");
    appendLog("关闭FIFO线程与读取线程");
}
