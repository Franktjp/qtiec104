#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logmsg.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // 1. control
    ui->setupUi(this);
    //      IP control
    ui->leIPAddr->setInputMask("000.000.000.000;_");
    ui->pbConnect->setText("连接");
    ui->leIPAddr->setText("127.000.000.001");


    // 2. connect

    connect(ui->pbConnect, &QPushButton::clicked, this, &MainWindow::slotPbConnectClicked);


    connect(&iec, &QIec104::signalTcpDisconnect, this, &MainWindow::slotTcpDisconnect);





    // 3. other members
    QString s;
    QTextStream(&s) << iec.getSlavePort();
    ui->lePort->setText(s);

    s = "";
    ui->leIPAddr->setText(s);





    // others

    iec.enableConnect();    // TODO: 先放在这里吧













    //    QRegularExpressionValidator val;

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotTimerLogMsg() {


}

void MainWindow::slotTcpConnect() {
    // 设置iec的ip地址、port
    iec.setSlavePort(ui->lePort->text().toInt());

    // TODO: 怎么报错了??
    //    iec.setSlaveIP(const_cast<char*>(ui->leIPAddr->text().toStdString().c_str()));


}

void MainWindow::slotDataIndication(struct iec_obj* pobj, unsigned int numpoints) {
    char buf[700];

    // TODO: 好复杂，好多


    switch (pobj->type) {
    case iec_base::M_BO_NA_1: {
        sprintf(buf, "%s%s%s%s%s", pobj->ov ? "ov " : "",
                pobj->bl ? "bl " : "", pobj->sb ? "sb " : "",
                pobj->nt ? "nt " : "", pobj->iv ? "iv " : "");
    }
        break;
    default: {


    }
        break;
    }
}

void MainWindow::slotCblogClicked() {
    // TODO: 这个check box log 怎么不见了
}

void MainWindow::slotPbConnectClicked() {
    if (iec.tm->isActive()) {   // if timer is running, stop connect
        iec.tm->stop();
        iec.tcp->close();
        iec.slotTcpDisconnect();
    } else {    // the timer is not running
        iec.setSlaveIP((char*)ui->leIPAddr->text().toStdString().c_str());
        iec.setSlavePort(ui->lePort->text().toUInt());

        ui->leIPAddr->setText(iec.getSlaveIP());
        QString s;
        QTextStream(&s) << iec.getSlavePort();
        ui->lePort->setText(s);

        ui->leIPAddr->setEnabled(false);
        ui->lePort->setEnabled(false);

        ui->pbConnect->setText("停止连接");
        ui->lbStatus->setText("<font color='blue'>正在连接</font>");

        // start the timer
        iec.tm->start(1000);

        iec.log.pushMsg("点击了连接");
        qDebug() << "点击了连接";
    }
}

void MainWindow::slotTcpDisconnect() {
    qDebug() << "this is slotTcpDisconnect";
    if (0) {
        // TODO:好多事情
        iec.disableConnect();
    }

    ui->lbStatus->setText("<font color='red'>连接已断开</font>");

    if (iec.tm->isActive()) {
        ui->pbConnect->setText("停止连接");
        ui->leIPAddr->setEnabled(false);
        ui->lePort->setEnabled(false);
    } else {
        ui->pbConnect->setText("连接");
        ui->leIPAddr->setEnabled(true);
        ui->lePort->setEnabled(true);
    }
}

