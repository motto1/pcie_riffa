/********************************************************************************
** Form generated from reading UI file 'testwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TESTWINDOW_H
#define UI_TESTWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TestWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QPushButton *btnCheckPcie;
    QPushButton *btnOpenPcie;
    QLineEdit *lineEditValue;
    QPushButton *btnPrepareSend;
    QPushButton *btnSendRecv;
    QPushButton *btnClosePcie;
    QTextEdit *textLog;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *TestWindow)
    {
        if (TestWindow->objectName().isEmpty())
            TestWindow->setObjectName("TestWindow");
        TestWindow->resize(600, 400);
        centralwidget = new QWidget(TestWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        btnCheckPcie = new QPushButton(centralwidget);
        btnCheckPcie->setObjectName("btnCheckPcie");

        horizontalLayout->addWidget(btnCheckPcie);

        btnOpenPcie = new QPushButton(centralwidget);
        btnOpenPcie->setObjectName("btnOpenPcie");

        horizontalLayout->addWidget(btnOpenPcie);

        lineEditValue = new QLineEdit(centralwidget);
        lineEditValue->setObjectName("lineEditValue");

        horizontalLayout->addWidget(lineEditValue);

        btnPrepareSend = new QPushButton(centralwidget);
        btnPrepareSend->setObjectName("btnPrepareSend");
        btnPrepareSend->setEnabled(false);

        horizontalLayout->addWidget(btnPrepareSend);

        btnSendRecv = new QPushButton(centralwidget);
        btnSendRecv->setObjectName("btnSendRecv");
        btnSendRecv->setEnabled(false);

        horizontalLayout->addWidget(btnSendRecv);

        btnClosePcie = new QPushButton(centralwidget);
        btnClosePcie->setObjectName("btnClosePcie");

        horizontalLayout->addWidget(btnClosePcie);


        verticalLayout->addLayout(horizontalLayout);

        textLog = new QTextEdit(centralwidget);
        textLog->setObjectName("textLog");
        textLog->setReadOnly(true);

        verticalLayout->addWidget(textLog);

        TestWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(TestWindow);
        statusbar->setObjectName("statusbar");
        TestWindow->setStatusBar(statusbar);

        retranslateUi(TestWindow);

        QMetaObject::connectSlotsByName(TestWindow);
    } // setupUi

    void retranslateUi(QMainWindow *TestWindow)
    {
        TestWindow->setWindowTitle(QCoreApplication::translate("TestWindow", "PCIE DLL \346\265\213\350\257\225\347\250\213\345\272\217", nullptr));
        btnCheckPcie->setText(QCoreApplication::translate("TestWindow", "\346\243\200\346\237\245\350\256\276\345\244\207", nullptr));
        btnOpenPcie->setText(QCoreApplication::translate("TestWindow", "\346\211\223\345\274\200\350\256\276\345\244\207", nullptr));
        lineEditValue->setPlaceholderText(QCoreApplication::translate("TestWindow", "\350\276\223\345\205\245\345\200\274", nullptr));
        btnPrepareSend->setText(QCoreApplication::translate("TestWindow", "\345\207\206\345\244\207\345\217\221\351\200\201", nullptr));
        btnSendRecv->setText(QCoreApplication::translate("TestWindow", "\345\217\221\351\200\201\346\216\245\346\224\266", nullptr));
        btnClosePcie->setText(QCoreApplication::translate("TestWindow", "\345\205\263\351\227\255\350\256\276\345\244\207", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TestWindow: public Ui_TestWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TESTWINDOW_H