#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qiec104.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private:
    Ui::MainWindow *ui;
    QIec104 iec;

private slots:
    void slotTimerLogMsg(); // timer for logmsg
    void slotCblogClicked();    //
    void slotTcpConnect();  // tcp connect for iec104
    void slotTcpDisconnect();
    void slotDataIndication(struct iec_obj*, unsigned int); //
    void slotPbConnectClicked();


signals:





};
#endif // MAINWINDOW_H
