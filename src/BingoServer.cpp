#include "BingoServer.h"
#include "BingoTicketParser.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

BingoServer::BingoServer(quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(new QWebSocketServer(QStringLiteral("BingoSys Server"),
                                            QWebSocketServer::NonSecureMode, this)),
    m_port(port)
{
}

BingoServer::~BingoServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

bool BingoServer::start()
{
    if (m_pWebSocketServer->listen(QHostAddress::Any, m_port)) {
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &BingoServer::onNewConnection);
        
        // Carrega cartelas ao iniciar (Temporario para teste)
        // Idealmente isso vem de um comando ou config
        QString dataPath = "data/base-cartelas.txt";
        // Ajuste path absoluto se necessario
        dataPath = "d:/PROJETOS/bingosys-server-ws/data/base-cartelas.txt";
        auto tickets = BingoTicketParser::parseFile(dataPath);
        m_gameEngine.loadTickets(tickets);
        m_gameEngine.setGameMode(0); // Default part 1
        
        return true;
    }
    return false;
}

void BingoServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &BingoServer::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived, this, &BingoServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &BingoServer::socketDisconnected);

    m_clients << pSocket;
    qInfo() << "Novo cliente conectado:" << pSocket->peerAddress().toString();
}

void BingoServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    // Tenta parsear como JSON
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        handleJsonMessage(pClient, doc.object());
    } else {
        qWarning() << "Mensagem nao-JSON recebida:" << message;
    }
}

void BingoServer::sendJson(QWebSocket *client, const QJsonObject &json)
{
    if (client && client->isValid()) {
        QJsonDocument doc(json);
        client->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
}

void BingoServer::broadcastJson(const QJsonObject &json)
{
    QJsonDocument doc(json);
    QString msg = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    for (QWebSocket *client : qAsConst(m_clients)) {
        client->sendTextMessage(msg);
    }
}

void BingoServer::handleJsonMessage(QWebSocket *client, const QJsonObject &json)
{
    QString action = json["action"].toString();
    
    if (action == "login") {
        // Exemplo simples de login
        QJsonObject response;
        response["action"] = "login_response";
        response["status"] = "ok";
        sendJson(client, response);
    } 
    else if (action == "start_game") {
        m_gameEngine.startNewGame();
        
        QJsonObject broadcast;
        broadcast["action"] = "game_started";
        broadcastJson(broadcast);
    }
    else if (action == "draw_number") {
        int number = json["number"].toInt();
        bool hasUpdates = m_gameEngine.processNumber(number);
        
        // Broadcast do numero sorteado
        QJsonObject numMsg;
        numMsg["action"] = "number_drawn";
        numMsg["number"] = number;
        broadcastJson(numMsg);
        
        if (hasUpdates) {
            // Envia atualizacao de ganhadores/armados
            QJsonObject updateMsg;
            updateMsg["action"] = "game_update";
            
            // Winners
            QJsonArray winnersArray;
            for(int w : m_gameEngine.getWinners()) winnersArray.append(w);
            updateMsg["winners"] = winnersArray;
            
            // Near Win (vamos mandar apenas faltam 1 por enquanto para economizar banda se quiser)
            // Ou mandar tudo estruturado
            QJsonObject nearWinObj;
            auto nearWins = m_gameEngine.getNearWinTickets();
            
            // Exemplo: "1": [id1, id2], "2": [id3...]
            for(auto it = nearWins.begin(); it != nearWins.end(); ++it) {
                QJsonArray idsInfo;
                for(int id : it.value()) idsInfo.append(id);
                nearWinObj[QString::number(it.key())] = idsInfo;
            }
            updateMsg["near_wins"] = nearWinObj;
            
            broadcastJson(updateMsg);
        }
    }
}

void BingoServer::processBinaryMessage(QByteArray message)
{
    Q_UNUSED(message);
    // Placeholder para binarios se necessario
}

void BingoServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        qInfo() << "Cliente desconectado:" << pClient->peerAddress().toString();
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}
