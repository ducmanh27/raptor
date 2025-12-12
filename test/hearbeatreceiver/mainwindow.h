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
    void checkConnectionTimeout();

private:
    // UI Components
    QTextEdit *logTextEdit;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QLineEdit *ipLineEdit;
    QLineEdit *portLineEdit;
    QLabel *statusLabel;
    QLabel *lastPingLabel;

    // Network
    QTcpSocket *tcpSocket;

    // Timer
    QTimer *connectionCheckTimer; // Check timeout mỗi 50ms

    // Connection tracking
    qint64 lastTimeConnection;    // Thời điểm nhận ping cuối cùng (ms)
    int connectionFailedCount;    // Đếm số lần timeout liên tiếp

    // Statistics
    int totalPingReceived;
    int totalResponseSent;

    void setupUI();
    void log(const QString &message);
    void sendResponse(quint32 seq, qint64 originalTimestamp);
    qint64 currentTimestamp();
};

#endif // MAINWINDOW_H
