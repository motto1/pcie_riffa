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
#include <QtWidgets/QCheckBox>
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
    QPushButton *btnStartFifo;
    QPushButton *btnToggleFifo;
    QPushButton *btnClosePcie;
    QHBoxLayout *horizontalLayout_2;
    QCheckBox *checkBoxRiffaLog;
    QCheckBox *checkBoxPiceDllLog;
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

        btnStartFifo = new QPushButton(centralwidget);
        btnStartFifo->setObjectName("btnStartFifo");
        btnStartFifo->setEnabled(false);

        horizontalLayout->addWidget(btnStartFifo);

        btnToggleFifo = new QPushButton(centralwidget);
        btnToggleFifo->setObjectName("btnToggleFifo");

        horizontalLayout->addWidget(btnToggleFifo);

        btnClosePcie = new QPushButton(centralwidget);
        btnClosePcie->setObjectName("btnClosePcie");

        horizontalLayout->addWidget(btnClosePcie);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        checkBoxRiffaLog = new QCheckBox(centralwidget);
        checkBoxRiffaLog->setObjectName("checkBoxRiffaLog");
        checkBoxRiffaLog->setChecked(false);

        horizontalLayout_2->addWidget(checkBoxRiffaLog);

        checkBoxPiceDllLog = new QCheckBox(centralwidget);
        checkBoxPiceDllLog->setObjectName("checkBoxPiceDllLog");
        checkBoxPiceDllLog->setChecked(true);

        horizontalLayout_2->addWidget(checkBoxPiceDllLog);


        verticalLayout->addLayout(horizontalLayout_2);

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
        btnStartFifo->setText(QCoreApplication::translate("TestWindow", "\345\220\257\345\212\250FIFO", nullptr));
        btnToggleFifo->setText(QCoreApplication::translate("TestWindow", "\345\205\263\351\227\255\347\272\277\347\250\213", nullptr));
        btnClosePcie->setText(QCoreApplication::translate("TestWindow", "\345\205\263\351\227\255\350\256\276\345\244\207", nullptr));
        checkBoxRiffaLog->setText(QCoreApplication::translate("TestWindow", "\345\220\257\347\224\250RIFFA\346\227\245\345\277\227", nullptr));
        checkBoxPiceDllLog->setText(QCoreApplication::translate("TestWindow", "\345\220\257\347\224\250PICE DLL\346\227\245\345\277\227", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TestWindow: public Ui_TestWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TESTWINDOW_H