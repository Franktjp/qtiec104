#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QClipboard>
#include <QMainWindow>
#include <QMessageBox>
#include "qiec104.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow* ui;
    QIec104 iec;
    QTimer* logTimer;  // timer for log

private slots:
    void slotLogTimerTimeout();   // timer for logger
    void slotCblogClicked();  //
    void slotTcpConnect();    // tcp connect has established
    void slotTcpDisconnect();
    void slotDataIndication(struct iec_obj*, unsigned int);  //
    void slotPbConnectClicked();    // connect to server
    void slotPbCopyLogClicked();    // copy log
    void slotPbParseClicked();      // parse message inputted from lineText
    void slotPbSendClicked();       // send message inputted from lineText

    void slotPbSendStartDtAct();            // send U启动帧
    void slotPbGeneralInterrogationAct();   // send 总召唤激活帧

signals:
};
#endif  // MAINWINDOW_H
