#include "mainwindow.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <chrono>
using namespace std::chrono_literals;
// Heartbeat packet structure (must match App1)
#pragma pack(push, 1)
struct HeartbeatPacket {
    quint32 magic;      // 0xABCDDCBA
    quint32 seq;        // Sequence number
    qint64 timestamp;   // Timestamp in ms
    char type;          // 'P' = Ping, 'R' = Response
    char data[100];     // Payload
};
#pragma pack(pop)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpSocket(nullptr)
    , lastTimeConnection(0)
    , connectionFailedCount(0)
    , totalPingReceived(0)
    , totalResponseSent(0)
{
    setupUI();

    // Initialize TCP socket
    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onError);
    // Connection check timer - check mỗi 50ms
    connectionCheckTimer = new QTimer(this);
    connect(connectionCheckTimer, &QTimer::timeout, this, &MainWindow::checkConnectionTimeout);
    connectionCheckTimer->start(50ms);

    log("App2 - Heartbeat Responder started");
}

MainWindow::~MainWindow()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->disconnectFromHost();
    }
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Connection group
    QGroupBox *connectionGroup = new QGroupBox("Connection", this);
    QHBoxLayout *connectionLayout = new QHBoxLayout(connectionGroup);

    connectionLayout->addWidget(new QLabel("IP:", this));
    ipLineEdit = new QLineEdit("192.168.49.53", this);  // ESP32 Ethernet IP
    ipLineEdit->setFixedWidth(120);
    connectionLayout->addWidget(ipLineEdit);

    connectionLayout->addWidget(new QLabel("Port:", this));
    portLineEdit = new QLineEdit("8888", this);  // ESP32 Ethernet port
    portLineEdit->setFixedWidth(60);
    connectionLayout->addWidget(portLineEdit);

    connectButton = new QPushButton("Connect", this);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectToServer);
    connectionLayout->addWidget(connectButton);

    disconnectButton = new QPushButton("Disconnect", this);
    disconnectButton->setEnabled(false);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectFromServer);
    connectionLayout->addWidget(disconnectButton);

    connectionLayout->addStretch();
    mainLayout->addWidget(connectionGroup);

    // Status group
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    statusLabel = new QLabel("Status: Disconnected", this);
    statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(statusLabel);

    lastPingLabel = new QLabel("Last Ping: N/A", this);
    statusLayout->addWidget(lastPingLabel);

    mainLayout->addWidget(statusGroup);

    // Log group
    QGroupBox *logGroup = new QGroupBox("Log", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);
    logLayout->addWidget(logTextEdit);

    mainLayout->addWidget(logGroup);

    setWindowTitle("App2 - Heartbeat Responder");
    resize(800, 600);
}

void MainWindow::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

qint64 MainWindow::currentTimestamp()
{
    return QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::connectToServer()
{
    QString ip = ipLineEdit->text();
    quint16 port = portLineEdit->text().toUShort();

    log(QString("Connecting to %1:%2...").arg(ip).arg(port));

    // Set TCP_NODELAY before connecting
    tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    tcpSocket->connectToHost(ip, port);

    connectButton->setEnabled(false);
}

void MainWindow::disconnectFromServer()
{
    log("Disconnecting...");
    tcpSocket->disconnectFromHost();
}

void MainWindow::onConnected()
{
    log("Connected to server!");

    statusLabel->setText("Status: Connected");
    statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");

    connectButton->setEnabled(false);
    disconnectButton->setEnabled(true);

    // Reset counters
    totalPingReceived = 0;
    totalResponseSent = 0;
    connectionFailedCount = 0;
    lastTimeConnection = currentTimestamp();

    log("Waiting for heartbeat...");
}

void MainWindow::onDisconnected()
{
    log("Disconnected from server");

    statusLabel->setText("Status: Disconnected");
    statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
}

void MainWindow::onReadyRead()
{
    while (tcpSocket->bytesAvailable() >= static_cast<qint64>(sizeof(HeartbeatPacket))) {
        HeartbeatPacket packet;
        qint64 bytesRead = tcpSocket->read(reinterpret_cast<char*>(&packet), sizeof(packet));

        if (bytesRead == sizeof(HeartbeatPacket)) {
            // Verify magic number
            if (packet.magic != 0xABCDDCBA) {
                log("Invalid packet magic number");
                continue;
            }

            // Check if it's a ping
            if (packet.type == 'P') {
                qint64 now = currentTimestamp();

                // Update last connection time - QUAN TRỌNG!
                lastTimeConnection = now;
                connectionFailedCount = 0;
                totalPingReceived++;

                log(QString("Ping received: Seq=%1")
                        .arg(packet.seq));

                lastPingLabel->setText(QString("Last Ping: %1ms ago (Recv: %2, Sent: %3)")
                                           .arg(0)
                                           .arg(totalPingReceived)
                                           .arg(totalResponseSent));

                // Send response immediately
                sendResponse(packet.seq, packet.timestamp);
            }
        }
    }
}

void MainWindow::onError(QAbstractSocket::SocketError socketError)
{
    log(QString("Socket error: %1").arg(tcpSocket->errorString()));

    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
}

void MainWindow::sendResponse(quint32 seq, qint64 originalTimestamp)
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    HeartbeatPacket packet;
    packet.magic = 0xABCDDCBA;
    packet.seq = seq;  // Echo back the same sequence number
    packet.timestamp = originalTimestamp;  // Keep original timestamp for RTT calculation
    packet.type = 'R';  // Response
    snprintf(packet.data, sizeof(packet.data), "Response to #%u", seq);

    qint64 bytesWritten = tcpSocket->write(reinterpret_cast<const char*>(&packet), sizeof(packet));

    if (bytesWritten == sizeof(HeartbeatPacket)) {
        totalResponseSent++;
        log(QString("Response sent: Seq=%1, Total=%2")
                .arg(seq)
                .arg(totalResponseSent));
    } else {
        log("Failed to send response");
    }
}

void MainWindow::checkConnectionTimeout()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    qint64 now = currentTimestamp();
    qint64 timeSinceLastPing = now - lastTimeConnection;

    // Update label
    lastPingLabel->setText(QString("Last Ping: %1ms ago (Recv: %2, Sent: %3)")
                               .arg(timeSinceLastPing)
                               .arg(totalPingReceived)
                               .arg(totalResponseSent));

    if (timeSinceLastPing > 200) {
        connectionFailedCount++;

        log(QString("CONNECTION TIMEOUT! No ping for %1ms (Failed count: %2)")
                .arg(timeSinceLastPing)
                .arg(connectionFailedCount));

        lastPingLabel->setText(QString("TIMEOUT: %1ms (Failed: %2)")
                                   .arg(timeSinceLastPing)
                                   .arg(connectionFailedCount));
        lastPingLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");

        // Reset after logging
        lastTimeConnection = now;
    } else {
        lastPingLabel->setStyleSheet("");
    }
}
