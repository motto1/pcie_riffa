#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "riffa.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_oldCoutBuf(nullptr)
    , m_oldCerrBuf(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("PCIE接受CMAKE版本.11.25.加入计数 log完整打印");
    old_elapsedMicros=0;
    // 创建日志目录和文件
    QString logDir = QApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString logPath = QString("%1/operations_%2.log").arg(logDir).arg(currentDate);
    
    m_logFile.setFileName(logPath);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_logStream.setDevice(&m_logFile);

    timer = new QTimer(this);
    timer->stop();
    connect(timer, &QTimer::timeout, this, &MainWindow::executeSingleOperation);

    // 创建自定义缓冲区
    m_buffer = std::make_unique<CustomBuffer>([this](const QString& text) {
        appendToTextBrowser4(text);
    });
}

MainWindow::~MainWindow()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    timer->stop();
    if (m_fpgaThread) {
        m_fpgaThread->stopThread();
        m_fpgaThread->wait();
        delete m_fpgaThread;
    }
    delete ui;
}

void MainWindow::appendToTextBrowser4(const QString& text)
{
    if (ui && ui->textBrowser_4) {
        ui->textBrowser_4->append(text);
        ui->textBrowser_4->verticalScrollBar()->setValue(
            ui->textBrowser_4->verticalScrollBar()->maximum());
    }
}

/*********************************************************************************************************
* 函 数 名 ：openpcie
* 功能说明 ：打开PCIE接口并申请发送和接受数据内存
* 形   参  ：无
* 返 回 值 ：0->PCIE开启成功;1->PCIE开启失败
*********************************************************************************************************/
int MainWindow::openpcie()
{

    fpga_pcie_id = 0;
    fpga_pcie_chnl = 0;
    fpga = fpga_open(fpga_pcie_id);
    pcie_Send_Buffer=(unsigned int *)malloc(8*8);
    if (pcie_Send_Buffer == NULL)
    {
        goto close_error;
    }
    pcie_recv_Buffer=(unsigned int *)malloc(4096*4*8);
    if (pcie_recv_Buffer == NULL)
    {
        goto close_error;
    }
    return 0;
    close_error:
    fpga_close(fpga);
    return 1;
}
/*********************************************************************************************************
* 函 数 名 ：closepcie
* 功能说明 ：关闭PCIE接口
* 形   参  ：无
* 返 回 值 ：无
*********************************************************************************************************/
void MainWindow::closepcie()
{
    free(pcie_Send_Buffer);
    free(pcie_recv_Buffer);
    fpga_close(fpga);
}
/*********************************************************************************************************
* 函 数 名 ：checkpcie
* 功能说明 ：检测是否有PCIE设备存在
* 形   参  ：无
* 返 回 值 ：0->未检测到fpga_pcie设备;其他值->检测到的fpga_pcie设备个数
*********************************************************************************************************/
int MainWindow::checkpcie()
{
    if (fpga_list(&fpga_pcie_info) != 0)
        return 0;
    if(fpga_pcie_info.num_fpgas==0)
        return 0;
    else
        return fpga_pcie_info.num_fpgas;
}

void MainWindow::on_Btn_Check_clicked()
{
    int i=0;
    i=checkpcie();
    if(i==0)
        ui->Lab_Fpga_Nums->setText("未检测到Pcie设备！");
    else
        ui->Lab_Fpga_Nums->setText("检测到Pcie设备！");

    printf("Fpga_Nums:%d\n",i);
    fflush(stdout);
}





void MainWindow::on_btn_open_pcie_clicked()
{
    int i=0;
    i=openpcie();
    if(i==0)
    {
        printf("open_pcie_ok\r\n");
        //printf("fpga id is: %d\r\n",(*fpga).id);
        fflush(stdout);
        ui->lineEdit_2->setText("open_pcie_ok");
    }
    else
    {
        printf("open_pcie_err\r\n");
        fflush(stdout);
        ui->lineEdit_2->setText("open_pcie_err"); 
    }
}


void MainWindow::on_Btn_close_pcie_clicked()
{
    fpga_close(fpga);
    printf("close_pcie_ok\r\n");
    fflush(stdout);
    ui->lineEdit_3->setText("close_pcie_ok");
}


// void MainWindow::on_Btn_AD_Connect_Start_clicked()
// {
//     // 清空之前的输出
//     if (ui->textBrowser_4) {
//         ui->textBrowser_4->clear();
//     }
    
//     // 重定向标准输出和错误输出
//     m_oldCoutBuf = std::cout.rdbuf(m_buffer.get());
//     m_oldCerrBuf = std::cerr.rdbuf(m_buffer.get());
    
//     int sent;
//     int rece;
//     int i;
//     QString displayText;
//     bool ok;
//     int input_value = QInputDialog::getInt(this, 
//                                          "输入数值",
//                                          "请输入10进制数(0-4294967295):",
//                                          0, // 默认值
//                                          0, // 最小值
//                                          50000, // 最大值
//                                          1, // 步长
//                                          &ok);
//     if(!ok) {
//         QMessageBox::warning(this, "警告", "未输入有效数值!");
//         return;
//     }
    
//     // 打印输入值
//     printf("input_value: 0x%08x\n", input_value);
//     printf("input_value 10: %d\n", input_value);

//     fflush(stdout);
    
//     // 获取输入值的高16位和低16位
//     uint16_t high_word = (input_value >> 16) & 0xFFFF;  // 获取高16位
//     uint16_t low_word = input_value & 0xFFFF;          // 获取低16位
    
//     // 拼接赋值
//     pcie_Send_Buffer[0] = (low_word << 16) | 0x013c;   // 将低16位放在高16位位置，与013c拼接
//     pcie_Send_Buffer[1] = (0x3e00 << 16) | high_word;  // 将3e00放在高16位位置，与高16位拼接
//     printf("high_word: 0x%04x\n", high_word);
//     printf("low_word: 0x%04x\n", low_word);
//     fflush(stdout);
//     printf("pcie_Send_Buffer[0]: 0x%08x\n", pcie_Send_Buffer[0]);
//     printf("pcie_Send_Buffer[1]: 0x%08x\n", pcie_Send_Buffer[1]);
//     fflush(stdout);
//     // int buffer_num = QInputDialog::getInt(this, 
//     //                                     "输入包数量",
//     //                                     "请输入要接收的数据包数量:",
//     //                                     1, // 默认值
//     //                                     1, // 最小值 
//     //                                     100, // 最大值
//     //                                     1, // 步长
//     //                                     &ok);
//     // if(!ok) {
//     //     QMessageBox::warning(this, "警告", "未输入有效的数据包数量!");
//     //     return;
//     // }
//     // qDebug() << "数据包数量:" << buffer_num;

//     sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
//     for(i=0;i<8;i++)
//         qDebug()<<pcie_Send_Buffer[i];  //测试
        
//     if(sent!=0)
//     {
//         printf("Fpga_send_ok\r\n");
//         displayText.append("Fpga发送成功\n");
//         fflush(stdout);
//         for(i=0;i<sent;i++) {
//             printf("send[%d]:%x\r\n",i,pcie_Send_Buffer[i]);
//             displayText.append(QString("发送数据[%1]:%2\n").arg(i).arg(pcie_Send_Buffer[i], 0, 16));
//         }
//         fflush(stdout);
        
//         // FPGA接收操作
//         rece=fpga_recv(fpga, fpga_pcie_chnl, pcie_recv_Buffer, input_value * 6, 2500);
//         if(rece!=0)
//         {
//             printf("Fpga_recv_ok\r\n");
//             displayText.append("Fpga接收成功\n");
//             fflush(stdout);
            
//             // 原代码将32位数据分成高16位和低16位显示
//             // int channel0 = (pcie_recv_Buffer[0]&0xffff0000)>>16;
//             // int channel1 = (pcie_recv_Buffer[0]&0x0000ffff);
//             // int channel2 = (pcie_recv_Buffer[1]&0xffff0000)>>16;
//             // int channel3 = (pcie_recv_Buffer[1]&0x0000ffff);
//             // 打印和显示 input_value * 6 个32位数据
//             printf("接收到 %d 个32位数据也就是%d个16位数据:\n", input_value * 6, input_value * 3);
//             displayText.append(QString("接收到 %1 个32位数据也就是%2个包:\n").arg(input_value * 6).arg(input_value));
//             for(int i = 0; i < input_value * 6 + 2; i++) 
//             {
//                 printf("Buffer[%d]:0x%08x\r\n", i, pcie_recv_Buffer[i]);
//                 displayText.append(QString("Buffer[%1]:0x%2\n").arg(i).arg(pcie_recv_Buffer[i], 8, 16, QChar('0')));
//             }
            
//             fflush(stdout);
//         }
//         else
//         {
//             printf("Fpga_recv_err\r\n");
//             displayText.append("Fpga接收失败\n");
//             fflush(stdout);
//         }
//     }
//     else
//     {
//         printf("Fpga_send_err\r\n");
//         displayText.append("Fpga发送失败\n");
//         fflush(stdout);
//     }
    
//     // 恢复标准输出和错误输出
//     std::cout.rdbuf(m_oldCoutBuf);
//     std::cerr.rdbuf(m_oldCerrBuf);
    
//     ui->textBrowser->setText(displayText);
// }



void MainWindow::on_Btn_AD_Connect_Start_clicked()
{
    num=0;
    // 清空之前的输出
    if (ui->textBrowser_4) {
        ui->textBrowser_4->clear();
    }
    
    // 重定向标准输出和错误输出
    m_oldCoutBuf = std::cout.rdbuf(m_buffer.get());
    m_oldCerrBuf = std::cerr.rdbuf(m_buffer.get());
    
    bool ok;
    input_value = QInputDialog::getInt(this, 
                                         "输入数值",
                                         "请输入10进制数(0-4294967295):",
                                         0, 0, 50000, 1, &ok);
    if(!ok) {
        QMessageBox::warning(this, "警告", "未输入有效数值!");
        return;
    }
    // 获取输入值的高16位和低16位
    uint16_t high_word = (input_value >> 16) & 0xFFFF;
    uint16_t low_word = input_value & 0xFFFF;
    
    // 拼接赋值
    pcie_Send_Buffer[0] = (low_word << 16) | 0x013c;
    pcie_Send_Buffer[1] = (0x3e00 << 16) | high_word;
    ui->textBrowser->append("测试0");
    qDebug() << "测试0";
    ui->textBrowser->repaint();  // 强制重绘
    sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    // 在textBrowser中显示测试信息
    ui->textBrowser->append("测试1");
    ui->textBrowser->repaint();  // 强制重绘
    // 如果线程已经在运行，先停止它
    // if (m_fpgaThread) {
    //     m_fpgaThread->stopThread();
    //     m_fpgaThread->wait();
    //     delete m_fpgaThread;
    // }
    ui->textBrowser->append("测试2");
    ui->textBrowser->repaint();  // 强制重绘
    // 创建并启动新的FPGA线程
    m_fpgaThread = new FPGAThread(this, fpga, pcie_Send_Buffer, pcie_recv_Buffer, 
                                 sent, input_value, num, this);
    ui->textBrowser->append("测试3");
    ui->textBrowser->repaint();  // 强制重绘
    // 连接信号
    connect(m_fpgaThread, &FPGAThread::updateDisplay, ui->textBrowser, &QTextBrowser::append);
    connect(m_fpgaThread, &FPGAThread::clearDisplay, ui->textBrowser, &QTextBrowser::clear);
    ui->textBrowser->append("测试4");
    ui->textBrowser->repaint();  // 强制重绘
    // 启动线程
    m_fpgaThread->start();
}

void MainWindow::on_Btn_AD_Connect_Start_timer_clicked()
{
    num = 0;
    // 创建日志文件
    QString logDir = QApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString logPath = QString("%1/timer_operation_%2.log").arg(logDir).arg(timestamp);
    
    QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream logStream(&logFile);
        
        bool ok;
        input_value = QInputDialog::getInt(this, 
                                         "输入数值",
                                         "请输入10进制数(0-4294967295):",
                                         0, 0, 50000, 1, &ok);
        if(!ok) {
            logStream << "用户取消输入" << Qt::endl;
            return;
        }
         // 分配内存并初始化
        old_recv_Buffer = (unsigned int *)malloc(4*8);
        if (!old_recv_Buffer) {
            writeLog("内存分配失败");
            emit updateDisplay("内存分配失败");
            return;
        }
        old_recv_Buffer[0] = 0x00000000;
        
        // 获取输入值的高16位和低16位
        uint16_t high_word = (input_value >> 16) & 0xFFFF;
        uint16_t low_word = input_value & 0xFFFF;
        
        // 拼接赋值
        pcie_Send_Buffer[0] = (low_word << 16) | 0x013c;
        pcie_Send_Buffer[1] = (0x3e00 << 16) | high_word;
        
        logStream << QString("初始化发送缓冲区 - Buffer[0]:0x%1, Buffer[1]:0x%2\n")
            .arg(pcie_Send_Buffer[0], 8, 16, QChar('0'))
            .arg(pcie_Send_Buffer[1], 8, 16, QChar('0'));
        
        sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
        if (sent == 0) {
            logStream << "初始FPGA发送失败" << Qt::endl;
            return;
        }
        
        logStream << "定时器启动，间隔100us" << Qt::endl;
        m_loopTimer.start();  // 启动计时器
        m_lastElapsedMicros = m_loopTimer.nsecsElapsed() / 1000;  // 初始化为当前时间戳（微秒）
        
        // 设置定时器为最小间隔
        timer->setInterval(0);  // 设置为0让定时器尽可能快地触发
        timer->start();
        
        logFile.close();
    }
}

// 添加停止线程的方法（可以绑定到停止按钮）
void MainWindow::on_Btn_Stop_clicked()
{
    if (m_fpgaThread) {
        m_fpgaThread->stopThread();
        m_fpgaThread->wait();
        delete m_fpgaThread;
        m_fpgaThread = nullptr;
    }
}

// 实现FPGAThread类
FPGAThread::FPGAThread(MainWindow* mainWindow, fpga_t* fpga, unsigned int* send_buf, 
                       unsigned int* recv_buf, int sent_val, int input_val, 
                       long num, QObject* parent)
    : QThread(parent)
    , m_mainWindow(mainWindow)
    , m_fpga(fpga)
    , m_sendBuffer(send_buf)
    , m_recvBuffer(recv_buf)
    , m_sent(sent_val)
    , m_inputValue(input_val)
    , m_num(num)
    , m_isRunning(true)
{
    // 创建日志文件路径
    QString logDir = QApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 使用当前时间创建日志文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    m_logFilePath = QString("%1/fpga_thread_%2.log").arg(logDir).arg(timestamp);
    
    // 打开日志文件
    m_logFile.setFileName(m_logFilePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
        writeLog("=== FPGA线程启动 ===");
        writeLog(QString("输入值: 0x%1").arg(m_inputValue, 8, 16, QChar('0')));
    }
}

void FPGAThread::stopThread()
{
    QMutexLocker locker(&m_mutex);
    m_isRunning = false;
}

void FPGAThread::run()
{
    int lineCount = 0;
    unsigned int * old_recv_Buffer = nullptr;
    QElapsedTimer loopTimer;
    
    try {
        // 分配内存并初始化
        old_recv_Buffer = (unsigned int *)malloc(4*8);
        if (!old_recv_Buffer) {
            writeLog("内存分配失败");
            emit updateDisplay("内存分配失败");
            return;
        }
        old_recv_Buffer[0] = 0x00000000;
        
        while (m_isRunning) {
            loopTimer.start();
            
            // 检查FPGA和缓冲区指针
            if (!m_fpga || !m_sendBuffer || !m_recvBuffer || !old_recv_Buffer) {
                writeLog("检测到无效指针");
                emit updateDisplay("检测到无效指针");
                break;
            }
            
            if(m_sent != 0) {
                int rece = fpga_recv(m_fpga, 0, m_recvBuffer, m_inputValue * 6, 2500);
                
                if(rece != 0) {
                    // if(m_recvBuffer[11] == 1 || 
                    //    m_recvBuffer[5]-old_recv_Buffer[0]==m_inputValue || 
                    //    old_recv_Buffer[0]-m_recvBuffer[5]==11184800 - m_inputValue*2) 
                    if(1)
                    {
                        // // 保存当前值
                        // old_recv_Buffer[0] = m_recvBuffer[5];
                        // if(m_num % 10000 == 0) {
                        //     if (m_mainWindow) {
                        //         m_mainWindow->closepcie();
                        //         m_mainWindow->openpcie();
                        //         writeLog("FPGA重新打开");
                        //     }
                        // }
                        // 发送继续命令
                        
                        m_sendBuffer[0] = 0x0000023c;
                        m_sendBuffer[1] = 0x3e000000;
                        m_sent = fpga_send(m_fpga, 0, m_sendBuffer, 2, 0, 1, 25000);
                        
                    }
                    else {
                        QString logMessage = QString("数据不匹配 - Buffer[5]:0x%1, old_recv_Buffer[0]:0x%2")
                            .arg(m_recvBuffer[5], 8, 16, QChar('0'))
                            .arg(old_recv_Buffer[0], 8, 16, QChar('0'));
                        writeLog(logMessage);
                        emit updateDisplay(logMessage);
                        m_sent = 0;
                        lineCount++;
                    }
                } 
                else {
                    writeLog("FPGA接收失败");
                    emit updateDisplay("FPGA接收失败");
                    lineCount++;
                    m_sent = 0;  // 设置m_sent为0，这样下次循环会退出
                }
            } 
            else {
                writeLog("FPGA发送失败,终止线程");
                emit updateDisplay("FPGA发送失败,终止线程");
                lineCount++;
                m_num=0;
                break;
            }
            // usleep(0.1);
            m_num++;
            
            // 记录循环耗时
            qint64 elapsedMicros = loopTimer.nsecsElapsed() / 1000;
            if(m_num % 10000 == 0) {
                QString timeMessage = QString("第%1次循环完成, 耗时: %2微秒")
                    .arg(m_num).arg(elapsedMicros);
                emit updateDisplay(timeMessage);
                writeLog(timeMessage);
                lineCount++;
            }
            // 创建一个新的计时器,等待100微秒
            QElapsedTimer waitTimer;
            waitTimer.start();
            while(waitTimer.nsecsElapsed() < 100000) { // 100000纳秒 = 100微秒
                QThread::yieldCurrentThread(); // 让出CPU时间片,避免占用过多资源
            }
            // 清空显示
            if(lineCount >= 10000) {
                emit clearDisplay();
                emit updateDisplay("已清空显示区域");
                lineCount = 0;
            }
            
            // 添加短暂延时
            //  QThread::usleep(100);
        }
    }
    catch (const std::exception& e) {
        writeLog(QString("线程异常: %1").arg(e.what()));
        emit updateDisplay(QString("线程异常: %1").arg(e.what()));
    }
    catch (...) {
        writeLog("未知异常");
        emit updateDisplay("未知异常");
    }
    
    // 清理资源
    if (old_recv_Buffer) {
        free(old_recv_Buffer);
        old_recv_Buffer = nullptr;
    }
    
    writeLog("=== FPGA线程结束 ===");
    m_logFile.close();
}









void MainWindow::on_Btn_Correct_clicked()
{
    int sent;
    int rece;
    int i;
    QString displayText;
    pcie_Send_Buffer[0]=0x0004033c;
    pcie_Send_Buffer[1]=0x3e000001;
    //for(i=1;i<10;i++)
    // pcie_Send_Buffer[i]=i;
    sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
    if(sent!=0)
    {
        printf("Fpga_send_ok\r\n");
        displayText.append("FPGA发送成功\n");
        fflush(stdout);
        for(i=0;i<sent;i++) {
            printf("send[%d]:%x\r\n",i,pcie_Send_Buffer[i]);
            displayText.append(QString("发送[%1]:%2\n").arg(i).arg(pcie_Send_Buffer[i], 0, 16));
        }
        fflush(stdout);
        // rece=fpga_recv(fpga, fpga_pcie_chnl, pcie_recv_Buffer, 258, 2500);
        // // if(rece!=0)
        // // {
        // //     printf("Fpga_recv_ok\r\n");
        // //     displayText.append("FPGA接收成功\n");
        // //     fflush(stdout);
        // //     // for(i=0;i<rece/2;i++)
        // //     //      printf("rece[%d]:%d\r\n",i,(pcie_recv_Buffer[2*i]<<32)|(pcie_recv_Buffer[2*i+1]));
        // //     for(i=0;i<rece;i++) {
        // //         printf("rece[%d]:%d\r\n",i,pcie_recv_Buffer[i]);
        // //         displayText.append(QString("接收[%1]:%2\n").arg(i).arg(pcie_recv_Buffer[i]));
        // //     }
        // //     fflush(stdout);
        // // }
        // // else
        // // {
        // //     printf("Fpga_recv_err\r\n");
        // //     displayText.append("FPGA接收失败\n");
        // //     fflush(stdout);
        // // }
    }
    else
    {
        printf("Fpga_send_err\r\n");
        displayText.append("FPGA发送失败\n");
        fflush(stdout);
    }
    ui->textBrowser_3->setText(displayText);
}


void MainWindow::on_clean_textBrowser_clicked()
{
    // 检查textBrowser的行数,超过10000行就清空
    ui->textBrowser->clear();
}

// 修改 executeSingleOperation 函数
void MainWindow::executeSingleOperation()
{

    
    if (sent == 0) {
        writeLog("FPGA发送失败,终止线程");
        ui->textBrowser->append("FPGA发送失败,终止线程");
        num = 0;
        timer->stop();
        return;
    }
    
    // 接收数据
    int rece = fpga_recv(fpga, fpga_pcie_chnl, pcie_recv_Buffer, input_value * 6, 2500);
    if (rece != 0) {
        // 发送继续命令
        if(1) {
            pcie_Send_Buffer[0] = 0x0000023c;
            pcie_Send_Buffer[1] = 0x3e000000;
            sent = fpga_send(fpga, fpga_pcie_chnl, pcie_Send_Buffer, 2, 0, 1, 25000);
            
            if (sent == 0) {
                writeLog("FPGA发送失败,终止线程");
                ui->textBrowser->append("FPGA发送失败,终止线程");
                timer->stop();
                return;
            }
        } 
        else {
            QString logMessage = QString("数据不匹配 - Buffer[5]:0x%1, old_recv_Buffer[0]:0x%2")
                .arg(pcie_recv_Buffer[5], 8, 16, QChar('0'))
                .arg(old_recv_Buffer[0], 8, 16, QChar('0'));
            writeLog(logMessage);
            ui->textBrowser->append(logMessage);
            sent = 0;
        }
    } 
    else {
        writeLog("FPGA接收失败");
        ui->textBrowser->append("FPGA接收失败");
        sent = 0;
    }
    // 创建一个新的计时器,等待100微秒
    QElapsedTimer waitTimer;
    waitTimer.start();
    while(waitTimer.nsecsElapsed() < 100000) { // 100000纳秒 = 100微秒
        QThread::yieldCurrentThread(); // 让出CPU时间片,避免占用过多资源
    }
    num++;
    // 获取当前时间戳
    qint64 currentElapsedMicros = m_loopTimer.nsecsElapsed() / 1000;  // 转换为微秒
    // 计算时间间隔
    qint64 intervalMicros = currentElapsedMicros - m_lastElapsedMicros;
    // 更新上一次时间戳
    m_lastElapsedMicros = currentElapsedMicros;
    
    if(num % 10000 == 0) {
        QString timeMessage = QString("第%1次循环完成, 耗时: %2微秒")
            .arg(num)
            .arg(intervalMicros);  // 使用计算出的时间间隔
        writeLog(timeMessage);
        ui->textBrowser->append(timeMessage);
    }
}

void MainWindow::writeLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("[%1] %2").arg(timestamp).arg(message);
    
    if (m_logFile.isOpen()) {
        m_logStream << logMessage << Qt::endl;
        m_logStream.flush();
    }
}

void MainWindow::updateDisplay(const QString& message)
{
    if (ui && ui->textBrowser) {
        ui->textBrowser->append(message);
    }
}



void MainWindow::on_timer_stop_clicked()
{
    // 停止计时器
    timer->stop();
    
    // 记录停止时间和统计信息
    QString stopMessage = QString("计时器已停止,共执行 %1 次操作").arg(num);
    writeLog(stopMessage);
    ui->textBrowser->append(stopMessage);

}

