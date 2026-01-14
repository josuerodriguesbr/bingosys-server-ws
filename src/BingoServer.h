#ifndef BINGOSERVER_H
#define BINGOSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include "BingoGameEngine.h"

class BingoServer : public QObject
{
    Q_OBJECT
public:
    explicit BingoServer(quint16 port, QObject *parent = nullptr);
    ~BingoServer();

    bool start();

private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();

private:
    void sendJson(QWebSocket *client, const QJsonObject &json);
    void broadcastJson(const QJsonObject &json);
    void handleJsonMessage(QWebSocket *client, const QJsonObject &json);

    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    quint16 m_port;
    
    BingoGameEngine m_gameEngine;
};

#endif // BINGOSERVER_H
