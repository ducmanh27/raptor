// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef CLIENT_H
#define CLIENT_H

#include <QDialog>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QTcpSocket;
QT_END_NAMESPACE

class Client : public QDialog
{
    Q_OBJECT

public:
    explicit Client(QWidget *parent = nullptr);

private slots:
    void connectToServer();
    void disconnectFromServer();
    void sendData();
    void readData();
    void displayError(QAbstractSocket::SocketError socketError);
    void onConnected();
    void onDisconnected();
    void updateConnectionState();

private:
    QComboBox *hostCombo = nullptr;
    QLineEdit *portLineEdit = nullptr;
    QPushButton *connectButton = nullptr;
    QPushButton *disconnectButton = nullptr;

    QLineEdit *sendLineEdit = nullptr;
    QPushButton *sendButton = nullptr;
    QPushButton *clearButton = nullptr;

    QTextEdit *receiveTextEdit = nullptr;

    QTcpSocket *tcpSocket = nullptr;
};

#endif
