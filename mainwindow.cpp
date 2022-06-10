#include "mainwindow.h"
#include "logmsg.h"
#include "ui_mainwindow.h"
#include "utils.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    logTimer = new QTimer();

    // 1. control
    initUIControls();

    // 2. connect
    connect(ui->pbConnect, &QPushButton::clicked, this,
            &MainWindow::slotPbConnectClicked);
    connect(ui->pbParse, &QPushButton::clicked, this,
            &MainWindow::slotPbParseClicked);
    connect(ui->pbSend, &QPushButton::clicked, this,
            &MainWindow::slotPbSendClicked);
    connect(ui->pbCopyLog, &QPushButton::clicked, this,
            &MainWindow::slotPbCopyLogClicked);
    connect(ui->pbSendStartDtAct, &QPushButton::clicked, this,
            &MainWindow::slotPbSendStartDtAct);
    connect(ui->pbGeneralInterrogationAct, &QPushButton::clicked, this,
            &MainWindow::slotPbGeneralInterrogationAct);
    connect(ui->pbSendCommand, &QPushButton::clicked, this,
            &MainWindow::slotPbSendCommand);
    connect(&iec, &QIec104::signalTcpConnect, this,
            &MainWindow::slotTcpConnect);
    connect(&iec, &QIec104::signalTcpDisconnect, this,
            &MainWindow::slotTcpDisconnect);
    connect(logTimer, &QTimer::timeout, this, &MainWindow::slotLogTimerTimeout);

    // 3. iec, timer and ...
    iec.enableConnect();
    logTimer->start(100);
}

void MainWindow::initUIControls() {
    QString s;

    ui->setupUi(this);
    ui->pbConnect->setText("连接");
    ui->pbConnect->setEnabled(true);
    ui->pbSend->setEnabled(false);
    ui->pbSendStartDtAct->setEnabled(false);
    ui->pbGeneralInterrogationAct->setEnabled(false);
    ui->pbSendCommand->setEnabled(false);

    // ip address
    ui->leIPAddr->setInputMask("000.000.000.000;_");
    ui->leIPAddr->setText("" /* "127.000.000.001" */);
    // port
    QTextStream(&s) << iec.getSlavePort();
    ui->lePort->setText(s);
}

MainWindow::~MainWindow() {
    delete ui;
    delete logTimer;
}

void MainWindow::slotLogTimerTimeout() {
    while (iec.log.haveMsg()) {
        ui->lwLog->addItem(iec.log.pullMsg().c_str());
    }
}

void MainWindow::slotTcpConnect() {  // tcp connect has established
    // 设置iec的ip地址、port
    iec.setSlavePort(ui->lePort->text().toInt());

    ui->pbSendStartDtAct->setEnabled(true);
    ui->lbStatus->setText("<font color='green'>连接成功</font>");
    ui->lwLog->clear(); // 连接成功后清空日志
    ui->pbSendCommand->setEnabled(true);
    ui->pbSend->setEnabled(true);
    ui->pbConnect->setText("断开连接");
}

/**
 * @brief MainWindow::slotDataIndication
 * @param pobj
 * @param numpoints
 * TODO: 待定
 */
void MainWindow::slotDataIndication(struct iec_obj* pobj,
                                    unsigned int numpoints) {
    char buf[700];

    switch (pobj->type) {
    case iec_base::M_BO_NA_1: {
        sprintf(buf, "%s%s%s%s%s", pobj->ov ? "ov " : "",
                pobj->bl ? "bl " : "", pobj->sb ? "sb " : "",
                pobj->nt ? "nt " : "", pobj->iv ? "iv " : "");
    } break;
    default: {
    } break;
    }
}

void MainWindow::slotCblogClicked() {
    // TODO: 这个check box log 怎么不见了
}

/**
 * @brief MainWindow::slotPbConnectClicked - If the timer is running , stop the tcp connect, otherwise finish tcp connect
 */
void MainWindow::slotPbConnectClicked() {
    ui->lwLog->clear();

    if (iec.tm->isActive()) {  // if timer is running, stop connect
        iec.tm->stop();
        iec.tcp->close();
        iec.disableConnect();
        iec.slotTcpDisconnect();
    } else {  // the timer is not running
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

        iec.enableConnect();
        // start the timer
        iec.tm->start(1000);

        char buf[100];
        sprintf(buf, "INFO: try to connect");
        iec.log.pushMsg(buf);
    }
}

void MainWindow::slotTcpDisconnect() {
    ui->lbStatus->setText("<font color='red'>连接已断开</font>");

    // NOTE: set all sent pushButton unenabled.
    ui->pbSendStartDtAct->setEnabled(false);
    ui->pbGeneralInterrogationAct->setEnabled(false);
    ui->pbSendCommand->setEnabled(false);
    ui->pbSend->setEnabled(false);

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

void MainWindow::slotPbCopyLogClicked() {
    int cnt, i;
    QStringList sl;

    cnt = ui->lwLog->count();
    for (i = 0; i < cnt; ++i) {
        sl << ui->lwLog->item(i)->text();
    }

    QApplication::clipboard()->setText(sl.join("\n"));

    // show the message box
    QMessageBox::information(this, "info", "Log copied to clipboard!");
}

void MainWindow::slotPbParseClicked() {
    int size = ui->leText->text().size();
    char buf[100];

    if (size % 2) {
        sprintf(buf, "The message of LineText is wrong");
        iec.log.pushMsg(buf);
        return;
    }

    uint8_t* binary = new uint8_t[size / 2];
    byteString2BinaryString(ui->leText->text(), binary);
    // Such as, U message: 680407000000
    iec.showMessage((char*)binary, size / 2, true);
    iec.parse((struct apdu*)binary, size / 2, false);
    delete[] binary;
}

void MainWindow::slotPbSendClicked() {
    int size = ui->leText->text().size();
    char buf[100];

    if (size % 2) {
        sprintf(buf, "The message is wrong");
        iec.log.pushMsg(buf);
        return;
    }

    sprintf(buf, "send message [%s]", ui->leText->text().toStdString().c_str());
    iec.log.pushMsg(buf);

    uint8_t* binary = new uint8_t[size / 2];
    byteString2BinaryString(ui->leText->text(), binary);
    iec.parse((struct apdu*)binary, size / 2, true);

    delete[] binary;
}

void MainWindow::slotPbSendStartDtAct() {
    iec.sendStartDtAct();
    ui->pbGeneralInterrogationAct->setEnabled(true);
    ui->pbSendStartDtAct->setEnabled(false);
}

void MainWindow::slotPbGeneralInterrogationAct() {
    iec.generalInterrogationAct();
}

/**
 * @brief MainWindow::slotPbSendCommand - When pushButton sendCommand is
 * clicked.
 */
void MainWindow::slotPbSendCommand() {
    struct iec_obj obj;
    // set type
    obj.type = ui->cbCommandType->currentText()
            .left(ui->cbCommandType->currentText().indexOf(':'))
            .toUInt();

    // check and set command value and ioa address
    if (ui->leCommandAddress->text().trimmed() == "" ||
            ui->leCommandValue->text().trimmed() == "") {
        return;
    }
    obj.address = ui->leCommandAddress->text().toUInt();  // 信息对象地址ioa
    obj.value = ui->leCommandValue->text().toFloat();     //
    obj.cause = iec_base::ACTIVATION;
    // TODO:
    //    obj.ca = ...; // ASDU公共地址

    QDateTime cur = QDateTime::currentDateTime();

    switch (obj.type) {
    case QIec104::C_SC_NA_1:
    case QIec104::C_SC_TA_1: {
        // NOTE: scs只占1位，因此只有两个值0和1
        obj.scs = uint8_t(ui->leCommandValue->text().toUInt());
    } break;
    case QIec104::C_DC_NA_1:
    case QIec104::C_DC_TA_1: {
        obj.dcs = ui->leCommandValue->text().toUInt();
    } break;
    case QIec104::C_RC_NA_1:
    case QIec104::C_RC_TA_1: {
        obj.rcs = ui->leCommandValue->text().toUInt();
    } break;
    case QIec104::C_SE_NA_1:
    case QIec104::C_SE_TA_1: {
        obj.value = ui->leCommandValue->text().toUInt();
    } break;
    case QIec104::C_CS_NA_1: {
        obj.time.res1 = 0;
        obj.time.res2 = 0;
        obj.time.res3 = 0;
        obj.time.res4 = 0;
        obj.time.year = uint8_t(cur.date().year() % 100);
        obj.time.mon = uint8_t(cur.date().month());
        obj.time.dmon = uint8_t(cur.date().day());
        obj.time.dweek = uint8_t(cur.date().dayOfWeek());   // return 1 to 7
        obj.time.hour = uint8_t(cur.time().hour());
        obj.time.min = uint8_t(cur.time().minute());
        obj.time.mesc = uint16_t(cur.time().msec() /* 0 ~ 999 */ + cur.time().second() /* 0 ~ 59 */ * 1000);
        obj.time.iv = 0;
        obj.time.su = 0;
    }
        break;
        // TODO: more and more command
    default:
        break;
    }

    obj.se = ui->cbCommandSE->isChecked();  // S/E
    obj.qu = ui->cbCommandQU->currentText()
            .left(ui->cbCommandQU->currentText().indexOf(':'))
            .toUInt();

    iec.sendCommand(&obj);
}
