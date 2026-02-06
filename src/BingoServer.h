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
#include "BingoDatabaseManager.h"

struct ClientSession {
    int sorteioId;
    int chaveId;
    QString accessKey;
    bool isOperator; // Se acessou com a chave de gerenciamento
};

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
    void socketDisconnected();

private:
    struct GameInstance {
        BingoGameEngine *engine;
        int baseId;
        int modeloId;
    };

    void sendJson(QWebSocket *client, const QJsonObject &json);
    void broadcastToGame(int sorteioId, const QJsonObject &json);
    void handleJsonMessage(QWebSocket *client, const QJsonObject &json);
    QJsonObject getTicketDetailsJson(int sorteioId, int ticketId);
    QJsonObject getGameStatusJson(int sorteioId);
    
    // Inicializa ou retorna um motor para um sorteio específico
    BingoGameEngine* getEngine(int sorteioId);

    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    QMap<QWebSocket *, ClientSession> m_sessions;
    quint16 m_port;
    
    QMap<QString, QVector<BingoTicket>> m_ticketCache;
    QMap<int, GameInstance> m_gameInstances;
    BingoDatabaseManager *m_db;
    
    QString m_masterToken;
    int m_historyLimit;
};

#endif // BINGOSERVER_H
