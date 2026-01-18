#include "BingoServer.h"
#include "BingoTicketParser.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

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
        return true;
    }
    return false;
}

void BingoServer::loadTickets(const QVector<BingoTicket> &tickets)
{
    m_gameEngine.loadTickets(tickets);
    m_gameEngine.setGameMode(0); // Default part 1
}

void BingoServer::setSalesPath(const QString &path)
{
    m_salesPath = path;
}

void BingoServer::loadSales()
{
    if (m_salesPath.isEmpty()) return;
    QFile file(m_salesPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(',');
        int id = parts[0].toInt();
        if (id > 0) {
            m_gameEngine.registerTicket(id);
            if (parts.size() > 1) {
                m_saleTimestamps[id] = parts[1];
            } else {
                m_saleTimestamps[id] = "N/A";
            }
        }
    }
    file.close();
    qInfo() << "Vendas carregadas:" << m_gameEngine.getRegisteredCount();
}

void BingoServer::saveSales()
{
    if (m_salesPath.isEmpty()) return;
    QFile file(m_salesPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    for (int id : m_gameEngine.getRegisteredTickets()) {
        QString ts = m_saleTimestamps.value(id, "N/A");
        out << id << "," << ts << "\n";
    }
    file.close();
}

void BingoServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &BingoServer::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived, this, &BingoServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &BingoServer::socketDisconnected);

    m_clients << pSocket;
    qInfo() << "Novo cliente conectado:" << pSocket->peerAddress().toString();

    // Envia estado inicial para sincronizacao
    QJsonObject sync;
    sync["action"] = "sync_status";
    sync["totalRegistered"] = m_gameEngine.getRegisteredCount();
    
    QJsonArray drawnArray;
    for(int n : m_gameEngine.getDrawnNumbers()) drawnArray.append(n);
    sync["drawnNumbers"] = drawnArray;

    QJsonArray winnersArray;
    for(int w : m_gameEngine.getWinners()) winnersArray.append(m_gameEngine.getFormattedBarcode(w));
    sync["winners"] = winnersArray;

    QJsonObject nearWinObj;
    auto nearWins = m_gameEngine.getNearWinTickets();
    for(auto it = nearWins.begin(); it != nearWins.end(); ++it) {
        QJsonArray idsInfo;
        for(int id : it.value()) idsInfo.append(m_gameEngine.getFormattedBarcode(id));
        nearWinObj[QString::number(it.key())] = idsInfo;
    }
    sync["near_wins"] = nearWinObj;

    sendJson(pSocket, sync);
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
            for(int w : m_gameEngine.getWinners()) winnersArray.append(m_gameEngine.getFormattedBarcode(w));
            updateMsg["winners"] = winnersArray;
            
            QJsonObject nearWinObj;
            auto nearWins = m_gameEngine.getNearWinTickets();
            for(auto it = nearWins.begin(); it != nearWins.end(); ++it) {
                QJsonArray idsInfo;
                for(int id : it.value()) idsInfo.append(m_gameEngine.getFormattedBarcode(id));
                nearWinObj[QString::number(it.key())] = idsInfo;
            }
            updateMsg["near_wins"] = nearWinObj;
            
            broadcastJson(updateMsg);
        }
    }
    else if (action == "undo_last") {
        int cancelledNum = m_gameEngine.undoLastNumber();
        if (cancelledNum != -1) {
            QJsonObject broadcast;
            broadcast["action"] = "number_cancelled";
            broadcast["number"] = cancelledNum;
            
            // Winners e NearWins atualizados
            QJsonArray winnersArray;
            for(int w : m_gameEngine.getWinners()) winnersArray.append(m_gameEngine.getFormattedBarcode(w));
            broadcast["winners"] = winnersArray;

            QJsonObject nearWinObj;
            auto nearWins = m_gameEngine.getNearWinTickets();
            for(auto it = nearWins.begin(); it != nearWins.end(); ++it) {
                QJsonArray idsInfo;
                for(int id : it.value()) idsInfo.append(m_gameEngine.getFormattedBarcode(id));
                nearWinObj[QString::number(it.key())] = idsInfo;
            }
            broadcast["near_wins"] = nearWinObj;
            
            broadcastJson(broadcast);
        }
    }
    else if (action == "register_ticket") {
        int barcode = json["barcode"].toInt(); 
        int ticketId = barcode / 10;
        int checkDigit = barcode % 10;
        
        QJsonObject resp;
        
        // Verifica se o dígito verificador é válido
        if (m_gameEngine.isValidCheckDigit(ticketId, checkDigit)) {
            m_gameEngine.registerTicket(ticketId);
            
            // Registra o timestamp da venda
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss");
            m_saleTimestamps[ticketId] = timestamp;
            
            resp["action"] = "ticket_registered";
            resp["status"] = "ok";
            resp["ticketId"] = m_gameEngine.getFormattedBarcode(ticketId);
            resp["timestamp"] = timestamp;
            resp["totalRegistered"] = m_gameEngine.getRegisteredCount();
            broadcastJson(resp);
            
            saveSales();
        } else {
            qWarning() << "Tentativa de registro com DV invalido:" << barcode;
            resp["action"] = "ticket_registered";
            resp["status"] = "error";
            resp["message"] = "Digito Verificador Invalido";
            sendJson(client, resp);
        }
    }
    else if (action == "register_random") {
        int count = json["count"].toInt();
        if (count <= 0) count = 10;
        
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss");
        
        const QVector<BingoTicket> &all = m_gameEngine.getAllTickets();
        if (!all.isEmpty()) {
            for (int i = 0; i < count; ++i) {
                int randomIndex = qrand() % all.size();
                int id = all[randomIndex].id;
                m_gameEngine.registerTicket(id);
                m_saleTimestamps[id] = timestamp + " (Lote)";
            }
        }
        
        QJsonObject resp;
        resp["action"] = "ticket_registered";
        resp["status"] = "ok";
        resp["ticketId"] = 0; 
        resp["totalRegistered"] = m_gameEngine.getRegisteredCount();
        broadcastJson(resp);
        
        saveSales();
    }
    else if (action == "clear_sales") {
        m_gameEngine.clearRegisteredTickets();
        m_saleTimestamps.clear();
        
        QJsonObject resp;
        resp["action"] = "sales_cleared";
        resp["totalRegistered"] = 0;
        broadcastJson(resp);
        
        saveSales();
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
