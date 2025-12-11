// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QtWidgets>
#include <QtNetwork>
#include "client.h"

Client::Client(QWidget *parent)
    : QDialog(parent)
    , hostCombo(new QComboBox)
    , portLineEdit(new QLineEdit)
    , connectButton(new QPushButton(tr("Connect")))
    , disconnectButton(new QPushButton(tr("Disconnect")))
    , sendLineEdit(new QLineEdit)
    , sendButton(new QPushButton(tr("Send")))
    , clearButton(new QPushButton(tr("Clear")))
    , receiveTextEdit(new QTextEdit)
    , tcpSocket(new QTcpSocket(this))
{
    // Setup host combo
    hostCombo->setEditable(true);
    QString name = QHostInfo::localHostName();

    hostCombo->addItem(QString("192.168.49.53"));

    if (!name.isEmpty()) {
        hostCombo->addItem(name);
        QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty())
            hostCombo->addItem(name + QChar('.') + domain);
    }
    if (name != QLatin1String("localhost"))
        hostCombo->addItem(QString("localhost"));

    const QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (const QHostAddress &entry : ipAddressesList) {
        if (!entry.isLoopback())
            hostCombo->addItem(entry.toString());
    }
    for (const QHostAddress &entry : ipAddressesList) {
        if (entry.isLoopback())
            hostCombo->addItem(entry.toString());
    }

    portLineEdit->setValidator(new QIntValidator(1, 65535, this));
    portLineEdit->setText("8888");

    // Setup receive area
    receiveTextEdit->setReadOnly(true);
    receiveTextEdit->setMaximumHeight(300);

    // Setup send area
    sendLineEdit->setPlaceholderText(tr("Enter data to send..."));

    // Setup buttons initial state
    connectButton->setDefault(true);
    disconnectButton->setEnabled(false);
    sendButton->setEnabled(false);

    // Create labels
    auto hostLabel = new QLabel(tr("&Server name:"));
    hostLabel->setBuddy(hostCombo);
    auto portLabel = new QLabel(tr("S&erver port:"));
    portLabel->setBuddy(portLineEdit);
    auto receiveLabel = new QLabel(tr("Received data:"));
    auto sendLabel = new QLabel(tr("Send data:"));

    // Create layouts
    QGridLayout *mainLayout = new QGridLayout(this);

    // Connection section
    mainLayout->addWidget(hostLabel, 0, 0);
    mainLayout->addWidget(hostCombo, 0, 1, 1, 2);
    mainLayout->addWidget(portLabel, 1, 0);
    mainLayout->addWidget(portLineEdit, 1, 1, 1, 2);
    mainLayout->addWidget(connectButton, 2, 0);
    mainLayout->addWidget(disconnectButton, 2, 1);

    // Receive section
    mainLayout->addWidget(receiveLabel, 3, 0, 1, 3);
    mainLayout->addWidget(receiveTextEdit, 4, 0, 1, 3);
    mainLayout->addWidget(clearButton, 5, 2);

    // Send section
    mainLayout->addWidget(sendLabel, 6, 0, 1, 3);
    mainLayout->addWidget(sendLineEdit, 7, 0, 1, 2);
    mainLayout->addWidget(sendButton, 7, 2);

    setWindowTitle(tr("TCP Client"));
    resize(500, 500);

    // Connect signals
    connect(connectButton, &QPushButton::clicked, this, &Client::connectToServer);
    connect(disconnectButton, &QPushButton::clicked, this, &Client::disconnectFromServer);
    connect(sendButton, &QPushButton::clicked, this, &Client::sendData);
    connect(clearButton, &QPushButton::clicked, receiveTextEdit, &QTextEdit::clear);
    connect(sendLineEdit, &QLineEdit::returnPressed, this, &Client::sendData);

    connect(tcpSocket, &QIODevice::readyRead, this, &Client::readData);
    connect(tcpSocket, &QAbstractSocket::errorOccurred, this, &Client::displayError);
    connect(tcpSocket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);
}

void Client::connectToServer()
{
    QString host = hostCombo->currentText();
    quint16 port = portLineEdit->text().toUInt();

    receiveTextEdit->append(tr("[%1] Connecting to %2:%3...")
                                .arg(QTime::currentTime().toString("HH:mm:ss"))
                                .arg(host)
                                .arg(port));

    tcpSocket->connectToHost(host, port);
    connectButton->setEnabled(false);
}

void Client::disconnectFromServer()
{
    receiveTextEdit->append(tr("[%1] Disconnecting...")
                                .arg(QTime::currentTime().toString("HH:mm:ss")));
    tcpSocket->disconnectFromHost();
}

void Client::sendData()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState)
        return;

    QString data = sendLineEdit->text();
    if (data.isEmpty())
        return;

    QByteArray byteArray = data.toUtf8();
    qint64 bytesWritten = tcpSocket->write(byteArray);

    receiveTextEdit->append(tr("[%1] Sent %2 bytes: %3")
                                .arg(QTime::currentTime().toString("HH:mm:ss"))
                                .arg(bytesWritten)
                                .arg(data));

    // sendLineEdit->clear();
}

void Client::readData()
{
    QByteArray data = tcpSocket->readAll();

    receiveTextEdit->append(tr("[%1] Received %2 bytes: %3")
                                .arg(QTime::currentTime().toString("HH:mm:ss"))
                                .arg(data.size())
                                .arg(QString::fromUtf8(data)));
}

void Client::displayError(QAbstractSocket::SocketError socketError)
{
    QString errorMsg;

    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        errorMsg = tr("Remote host closed the connection");
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = tr("Host not found. Please check the host name and port settings");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = tr("Connection refused. Make sure the server is running");
        break;
    default:
        errorMsg = tr("Error: %1").arg(tcpSocket->errorString());
    }

    receiveTextEdit->append(tr("[%1] ERROR: %2")
                                .arg(QTime::currentTime().toString("HH:mm:ss"))
                                .arg(errorMsg));

    updateConnectionState();
}

void Client::onConnected()
{
    receiveTextEdit->append(tr("[%1] Connected to %2:%3")
                                .arg(QTime::currentTime().toString("HH:mm:ss"))
                                .arg(tcpSocket->peerAddress().toString())
                                .arg(tcpSocket->peerPort()));

    updateConnectionState();
}

void Client::onDisconnected()
{
    receiveTextEdit->append(tr("[%1] Disconnected")
                                .arg(QTime::currentTime().toString("HH:mm:ss")));

    updateConnectionState();
}

void Client::updateConnectionState()
{
    bool connected = (tcpSocket->state() == QAbstractSocket::ConnectedState);

    connectButton->setEnabled(!connected);
    disconnectButton->setEnabled(connected);
    if  (connected)
    {
        disconnectButton->setFocus();
    }
    else
    {
        connectButton->setFocus();
    }
    sendButton->setEnabled(connected);
    sendLineEdit->setEnabled(connected);
    hostCombo->setEnabled(!connected);
    portLineEdit->setEnabled(!connected);
}
