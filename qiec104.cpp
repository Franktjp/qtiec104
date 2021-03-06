#include "qiec104.h"

QIec104::QIec104(QObject* parent) : QObject(parent) {
    this->isEnd = false;

    this->tm = new QTimer();
    this->log.activateLog();
    this->log.activateTime();
    initSocket();
}

QIec104::~QIec104() {
    delete this->tcp;
}

void QIec104::initSocket() {
    this->tcp = new QTcpSocket();
    connect(this->tcp, &QTcpSocket::connected, this, &QIec104::slotTcpConnect);
    connect(this->tcp, &QTcpSocket::disconnected, this,
            &QIec104::slotTcpDisconnect);
    connect(this->tcp, &QTcpSocket::readyRead, this,
            &QIec104::slotTcpReadyRead);
    connect(this->tcp, &QTcpSocket::errorOccurred, this,
            &QIec104::slotTcpError);
    // NOTE: 定时器是socket相关的，只有建立连接才有用
    connect(this->tm, &QTimer::timeout, this, &QIec104::slotTimeout);
}

void QIec104::terminate() {
    this->isEnd = true;
    this->tcp->close();
    // TODO:
}

void QIec104::enableConnect() {
    this->allowConnect = true;
}

void QIec104::disableConnect() {
    this->allowConnect = false;
    return;
    if (this->tcp->state() == QAbstractSocket::ConnectedState) {
        tcpDisconnect();
    }
}

void QIec104::tcpConnect() {
    if (this->isEnd || !this->allowConnect) {
        return;
    }
    this->tcp->close();
    this->log.deavtivateLog();
    this->log.activateLog();

    if (!this->isEnd && this->allowConnect) {
        this->tcp->connectToHost(getSlaveIP(), quint16(getSlavePort()),
                                 QIODevice::ReadWrite,
                                 QAbstractSocket::IPv4Protocol);
        char buf[100];
        sprintf(buf, "INFO: try to connect ip: %s, port: %d", getSlaveIP(), quint16(getSlavePort()));
        log.pushMsg(buf);
    }
}

void QIec104::tcpDisconnect() {
    tcp->close();
}

int QIec104::readTCP(char* buf, int size) {
    int ret;
    if (this->isEnd) {
        return 0;
    }
    ret = (int)tcp->read(buf, size);
    if (ret > 0) {
        return ret;
    }
    return 0;
}

void QIec104::sendTCP(const char* buf, int size) {
    if (this->tcp->state() == QAbstractSocket::ConnectedState) {
        this->tcp->write(buf, size);
        if (log.isLogging()) {
            showMessage(buf, size, true);
        }
    } else {
        if (log.isLogging()) {
            log.pushMsg("The tcp connection is not established!");
        }
    }
}

void QIec104::slotTcpConnect() {
    // NOTE: Is the option useful?
    tcp->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    onTcpConnect();
    emit this->signalTcpConnect();
}

void QIec104::slotTcpDisconnect() {
    onTcpDisconnect();
    emit this->signalTcpDisconnect();
}

void QIec104::slotTcpReadyRead() {
    int i = 0;
    if (this->tcp->bytesAvailable() < 6) {
        while (!this->tcp->waitForReadyRead(30) && i < 5) {  // delay 0.03 sec
            // readyRead() signal not be emitted
            ++i;
        }
    }
    messageReadyRead();
}

void QIec104::slotTcpError(QAbstractSocket::SocketError err) {
    //    if (err != QAbstractSocket::SocketTimeoutError) {
    char buf[100];
    sprintf(buf, "ERROR: socket error : %d(%s)", err,
            tcp->errorString().toStdString().c_str());
    log.pushMsg(buf);
    qDebug() << buf;
    //    }
}

void QIec104::slotTimeout() {
    static uint32_t cnt = 0;
    if (!this->isEnd) {
        if (!(cnt++ % 5)) {  // 每5s输出一次
            if (tcp->state() != QAbstractSocket::ConnectedState &&
                    this->allowConnect) {
                char buf[100];
                sprintf(buf, "INFO: try to connect ip: %s", getSlaveIP());
                log.pushMsg(buf);
                tcpConnect();
            }
        }

        // 每秒定时处理
        onTimeoutPerSecond();
    }
}

void QIec104::dataIndication(struct iec_obj* obj, unsigned int numpoints) {
    emit signalDataIndication(obj, numpoints);
}

void QIec104::udpConnect() {
    // TODO
}

void QIec104::udpDisconnect() {
    // TODO
}

int QIec104::bytesAvailable() {
    return int(tcp->bytesAvailable());
}

void QIec104::waitForReadyRead(int bytes, int msecs) {
    int i = 0;
    while (i < msecs && tcp->bytesAvailable() < bytes) {
        tcp->waitForReadyRead(10);
        i += 10;
    }
}
