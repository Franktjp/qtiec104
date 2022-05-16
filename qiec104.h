#ifndef QIEC104_H
#define QIEC104_H

#include <QObject>
#include <QTimer>
#include <QtNetwork/QTcpSocket>

#include "iec_base.h"

class QIec104 : public QObject, public iec_base {
    Q_OBJECT
public:
    explicit QIec104(QObject* parent = 0);
    ~QIec104();

    QTcpSocket* tcp;
    QTimer* tm;     // tmKeepAlive定时器(1 second)


public:

    void terminate();

    void enableConnect();
    void disableConnect();


private:
    // override base class virtual functions
    int readTCP(char* buf, int size);
    void sendTCP(const char* buf, int size);

    void tcpConnect();
    void tcpDisconnect();

    int bytesAvailable();
    void waitForReadyRead(int bytes, int msecs);

    void dataIndication(struct iec_obj* /* obj */, unsigned int /* numpoints */);

    // TODO
    void udpConnect();
    void udpDisconnect();



private:
    bool end;           // 是否终止
    bool allowConnect;  //



signals:
    void signalTcpCpnnect();
    void signalTcpDisconnect();
    void signalDataIndication(struct iec_obj*, unsigned int);


public slots:
    void slotTcpDisconnect();

private slots:
    void slotTcpConnect();
    void slotTcpReadyRead();    // ready to read via tcp socket
    void slotTcpError(QAbstractSocket::SocketError err);
    void slotTimeOut(); // when timer is
};

#endif // QIEC104_H
