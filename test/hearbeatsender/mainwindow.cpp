#include "mainwindow.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <chrono>

using namespace std::chrono_literals;
// Heartbeat packet structure
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
    , heartbeatSeq(0)
    , lastTimeConnection(0)
    , connectionFailedCount(0)
    , totalHeartbeatSent(0)
    , totalResponseReceived(0)
{
    setupUI();
    
    // Initialize TCP socket
    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onError);
    
    // Heartbeat timer - gửi mỗi 200ms
    heartbeatTimer = new QTimer(this);
    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendHeartbeat);
    
    // Connection check timer - check mỗi 50ms
    connectionCheckTimer = new QTimer(this);
    connect(connectionCheckTimer, &QTimer::timeout, this, &MainWindow::checkConnectionTimeout);
    connectionCheckTimer->start(50ms);
    
    log("App1 - Heartbeat Sender started");
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
    ipLineEdit = new QLineEdit("192.168.10.1", this);
    ipLineEdit->setFixedWidth(120);
    connectionLayout->addWidget(ipLineEdit);
    
    connectionLayout->addWidget(new QLabel("Port:", this));
    portLineEdit = new QLineEdit("9999", this);
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
    
    lastResponseLabel = new QLabel("Last Response: N/A", this);
    statusLayout->addWidget(lastResponseLabel);
    
    mainLayout->addWidget(statusGroup);
    
    // Log group
    QGroupBox *logGroup = new QGroupBox("Log", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    
    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);
    logLayout->addWidget(logTextEdit);
    
    mainLayout->addWidget(logGroup);
    
    setWindowTitle("App1 - Heartbeat Sender");
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
    heartbeatTimer->stop();
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
    heartbeatSeq = 0;
    totalHeartbeatSent = 0;
    totalResponseReceived = 0;
    connectionFailedCount = 0;
    lastTimeConnection = currentTimestamp();
    
    // Start heartbeat timer - 200ms interval
    heartbeatTimer->start(200ms);
    
    log("Heartbeat started (200ms interval)");
}

void MainWindow::onDisconnected()
{
    log("Disconnected from server");
    
    statusLabel->setText("Status: Disconnected");
    statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    
    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
    
    heartbeatTimer->stop();
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
            
            // Check if it's a response
            if (packet.type == 'R') {
                qint64 now = currentTimestamp();
                qint64 rtt = now - packet.timestamp;
                
                // Update last connection time
                lastTimeConnection = now;
                connectionFailedCount = 0;
                totalResponseReceived++;
                
                log(QString("Response received: Seq=%1, RTT=%2ms")
                    .arg(packet.seq)
                    .arg(rtt));
                
                lastResponseLabel->setText(QString("Last Response: %1ms ago (RTT: %2ms)")
                    .arg(0)
                    .arg(rtt));
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

void MainWindow::sendHeartbeat()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    HeartbeatPacket packet;
    packet.magic = 0xABCDDCBA;
    packet.seq = ++heartbeatSeq;
    packet.timestamp = currentTimestamp();
    packet.type = 'P';  // Ping
    snprintf(packet.data, sizeof(packet.data), "Heartbeat #%u", packet.seq);
    
    qint64 bytesWritten = tcpSocket->write(reinterpret_cast<const char*>(&packet), sizeof(packet));
    
    if (bytesWritten == sizeof(HeartbeatPacket)) {
        totalHeartbeatSent++;
        log(QString("Sent heartbeat: Seq=%1, Total=%2")
            .arg(packet.seq)
            .arg(totalHeartbeatSent));
    } else {
        log("Failed to send heartbeat");
    }
}

void MainWindow::checkConnectionTimeout()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    qint64 now = currentTimestamp();
    qint64 timeSinceLastResponse = now - lastTimeConnection;
    
    // Update label
    lastResponseLabel->setText(QString("Last Response: %1ms ago (Sent: %2, Recv: %3)")
        .arg(timeSinceLastResponse)
        .arg(totalHeartbeatSent)
        .arg(totalResponseReceived));

    if (timeSinceLastResponse > 2000) {
        connectionFailedCount++;
        
        log(QString("CONNECTION TIMEOUT! No response for %1ms (Failed count: %2)")
            .arg(timeSinceLastResponse)
            .arg(connectionFailedCount));
        
        lastResponseLabel->setText(QString("TIMEOUT: %1ms (Failed: %2)")
            .arg(timeSinceLastResponse)
            .arg(connectionFailedCount));
        lastResponseLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        
        // Reset after logging
        lastTimeConnection = now;
    } else {
        lastResponseLabel->setStyleSheet("");
    }
}
