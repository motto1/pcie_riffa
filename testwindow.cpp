#include "testwindow.h"
#include "ui_testwindow.h"
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>

TestWindow::TestWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TestWindow),
    pcie(new Pice_dll()),
    receiveBuffer(nullptr),
    logFile(nullptr),
    logStream(nullptr),
    workerThread(nullptr)
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
    if(workerThread) {
        workerThread->stop();
        workerThread->wait();
        delete workerThread;
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
    if(receiveBuffer) {
        delete[] receiveBuffer;
    }
    receiveBuffer = new unsigned int[4096 * 4];
    
    appendLog("正在打开PCIE设备...");
    if(pcie->openPcie(receiveBuffer) == 0) {
        appendLog("PCIE设备打开成功");
        ui->btnPrepareSend->setEnabled(true);
    } else {
        appendLog("打开PCIE设备失败: " + pcie->getLastError());
    }
}

void TestWindow::on_btnPrepareSend_clicked()
{
    bool ok;
    int value = ui->lineEditValue->text().toInt(&ok);
    if(!ok) {
        QMessageBox::warning(this, "错误", "请输入有效的数值");
        return;
    }
    
    appendLog(QString("准备发送数据，输入值: %1").arg(value));
    if(pcie->prepare_fpga_send(value)) {
        appendLog("数据准备完成");
        ui->btnSendRecv->setEnabled(true);
        
        // 创建并启动工作线程
        if(workerThread) {
            workerThread->stop();
            workerThread->wait();
            delete workerThread;
        }
        
        if(receiveBuffer) {
            delete[] receiveBuffer;
        }
        receiveBuffer = new unsigned int[4096 * 4];
        
        workerThread = new WorkerThread(pcie, receiveBuffer, this);
        connect(workerThread, &WorkerThread::logMessage, this, &TestWindow::onWorkerLogMessage);
        connect(workerThread, &WorkerThread::operationCompleted, this, &TestWindow::onOperationCompleted);
        
        appendLog("开始循环发送接收数据...");
        workerThread->start();
        
    } else {
        appendLog("数据准备失败: " + pcie->getLastError());
    }
}

void TestWindow::on_btnSendRecv_clicked()
{
    appendLog("开始循环发送接收数据...");
    int loopCount = 0;
    
    while(true) {
        QPair<bool, qint64> result = pcie->fpga_send_recv();
        if(!result.first) {
            appendLog("操作失败: " + pcie->getLastError());
            break;
        }
        
        loopCount++;
        
        // 每10000次打印一次执行信息
        if(loopCount % 10000 == 0) {
            appendLog(QString("第 %1 次执行完成，耗时: %2 微秒")
                .arg(loopCount)
                .arg(result.second));
            
            // 处理Qt事件，确保界面响应
            QApplication::processEvents();
        }
        
        // 检查是否需要停止循环
        if(!pcie->isConnected()) {
            appendLog("设备已断开，停止循环");
            break;
        }
    }
    
    appendLog("循环执行结束");
}

void TestWindow::on_btnClosePcie_clicked()
{
    appendLog("正在关闭PCIE设备...");
    
    if(workerThread) {
        workerThread->stop();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
    }
    
    pcie->closePcie();
    ui->btnPrepareSend->setEnabled(false);
    ui->btnSendRecv->setEnabled(false);
    appendLog("PCIE设备已关闭");
}

void TestWindow::onLogGenerated(const QString& message)
{
    // 获取当前的循环次数（从消息中提取）
    static int currentLoop = 0;
    if (message.contains("第") && message.contains("次执行完成")) {
        QRegularExpression rx("第\\s*(\\d+)\\s*次");
        QRegularExpressionMatch match = rx.match(message);
        if (match.hasMatch()) {
            currentLoop = match.captured(1).toInt();
        }
    }

    // 只在以下情况下记录RIFFA日志：
    // 1. 前10000次循环
    // 2. 834万次以后
    // 3. 包含错误信息
    if (currentLoop <= 10000 || 
        currentLoop >= 12420000 || 
        message.contains("失败") || 
        message.contains("错误")) {
        writeToLogFile("RIFFA: " + message);
    }
}

void TestWindow::onWorkerLogMessage(const QString& message)
{
    appendLog(message);
}

void TestWindow::onOperationCompleted(int count, qint64 time)
{
    appendLog(QString("第 %1 次执行完成，耗时: %2 微秒")
        .arg(count)
        .arg(time));
}

// 添加工作线程类的实现
WorkerThread::WorkerThread(Pice_dll* pcie, unsigned int* buffer, QObject* parent)
    : QThread(parent), m_pcie(pcie), m_buffer(buffer), m_running(true)
{
}

void WorkerThread::stop()
{
    m_running = false;
}

void WorkerThread::run()
{
    int loopCount = 0;
    const int LOG_THRESHOLD = 12420000;  // 834万次的阈值
    bool startLogging = false;  // 是否开始记录日志的标志
    
    while(m_running) {
        // 重新设置缓冲区
        if(m_pcie->openPcie(m_buffer) != 0) {
            // 只在失败时输出日志
            emit logMessage("打开pcie设备失败: " + m_pcie->getLastError());
            break;
        }
        
        QPair<bool, qint64> result = m_pcie->fpga_send_recv();
        if(!result.first) {
            // 只在失败时输出日志
            emit logMessage("操作失败: " + m_pcie->getLastError());
            break;
        }
        
        loopCount++;
        
        // 检查是否达到日志记录阈值
        if (loopCount >= LOG_THRESHOLD && !startLogging) {
            startLogging = true;
            emit logMessage(QString("\n====== 达到 %1 次，开始记录详细日志 ======\n").arg(LOG_THRESHOLD));
        }
        
        // 每10000次发送一次信号和记录日志
        if(loopCount % 10000 == 0) {
            // 发送执行完成信号，更新界面显示
            emit operationCompleted(loopCount, result.second);
        }
        
        // 检查是否需要停止循环
        if(!m_pcie->isConnected()) {
            emit logMessage("设备已断开，停止循环");
            break;
        }
    }
    
    emit logMessage("循环执行结束");
} 