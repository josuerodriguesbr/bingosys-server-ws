#ifndef BINGOSERVER_H
#define BINGOSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include "BingoGameEngine.h"

class BingoServer : public QObject
{
    Q_OBJECT
public:
    explicit BingoServer(quint16 port, QObject *parent = nullptr);
    ~BingoServer();

    bool start();
    void loadTickets(const QVector<BingoTicket> &tickets);
    void setSalesPath(const QString &path);
    void loadSales();
    void saveSales();

    void loadConfig();
    void saveConfig();

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
    QString m_salesPath;
    QString m_configPath;
    QJsonArray m_modes; // Lista de modos carregados do modes.json
    QMap<int, QString> m_saleTimestamps; // Mapeia ID da cartela para o timestamp da venda
};

#endif // BINGOSERVER_H
