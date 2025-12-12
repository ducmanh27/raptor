// ============================================
// APP1 - HEARTBEAT SENDER
// File: mainwindow.h
// ============================================

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void connectToServer();
    void disconnectFromServer();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void sendHeartbeat();
    void checkConnectionTimeout();

private:
    // UI Components
    QTextEdit *logTextEdit;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QLineEdit *ipLineEdit;
    QLineEdit *portLineEdit;
    QLabel *statusLabel;
    QLabel *lastResponseLabel;
    
    // Network
    QTcpSocket *tcpSocket;
    
    // Timers
    QTimer *heartbeatTimer;      // Gửi heartbeat mỗi 200ms
    QTimer *connectionCheckTimer; // Check timeout mỗi 50ms
    
    // Connection tracking
    qint64 lastTimeConnection;    // Thời điểm nhận response cuối cùng (ms)
    quint32 heartbeatSeq;         // Sequence number
    int connectionFailedCount;    // Đếm số lần timeout liên tiếp
    
    // Statistics
    int totalHeartbeatSent;
    int totalResponseReceived;
    
    void setupUI();
    void log(const QString &message);
    qint64 currentTimestamp();
};

#endif // MAINWINDOW_H