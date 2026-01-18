#include "BingoServer.h"
#include "BingoTicketParser.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRandomGenerator>

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
    // Define o caminho do config na mesma pasta
    QFileInfo info(path);
    m_configPath = info.absolutePath() + "/config.json";
    
    loadConfig();
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
    
    // Auto-inicializa o jogo para garantir que o motor esteja "ligado"
    m_gameEngine.startNewGame();
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

void BingoServer::loadConfig()
{
    if (m_configPath.isEmpty()) return;
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo() << "Config nao encontrado, usando padroes.";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj.contains("maxBalls")) m_gameEngine.setMaxBalls(obj["maxBalls"].toInt());
    if (obj.contains("gridIndex")) m_gameEngine.setGameMode(obj["gridIndex"].toInt());
    if (obj.contains("numChances")) m_gameEngine.setNumChances(obj["numChances"].toInt());

    qInfo() << "Configurações carregadas: MaxBalls=" << m_gameEngine.getMaxBalls() 
            << "Grade=" << m_gameEngine.getGameMode() 
            << "Chances=" << m_gameEngine.getNumChances();
}

void BingoServer::saveConfig()
{
    if (m_configPath.isEmpty()) return;
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonObject obj;
    obj["maxBalls"] = m_gameEngine.getMaxBalls();
    obj["gridIndex"] = m_gameEngine.getGameMode();
    obj["numChances"] = m_gameEngine.getNumChances();

    file.write(QJsonDocument(obj).toJson());
    file.close();
    qInfo() << "Configurações salvas em:" << m_configPath;
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
    
    // Configurações atuais
    sync["maxBalls"] = m_gameEngine.getMaxBalls();
    sync["gridIndex"] = m_gameEngine.getGameMode();
    sync["numChances"] = m_gameEngine.getNumChances();

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
        qInfo() << "Processando bola sorteada:" << number;
        bool hasUpdates = m_gameEngine.processNumber(number);
        
        // Broadcast do numero sorteado
        QJsonObject numMsg;
        numMsg["action"] = "number_drawn";
        numMsg["number"] = number;
        broadcastJson(numMsg);
        
        if (hasUpdates) {
            qInfo() << "Novas atualizacoes detectadas para a bola" << number;
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
        int baseId = barcode / 10;
        int checkDigit = barcode % 10;
        int numChances = m_gameEngine.getNumChances();
        
        qInfo() << "Tentativa de registro: Barcode" << barcode << "-> BaseID" << baseId << "DV" << checkDigit << "Chances" << numChances;
        
        QJsonObject resp;
        
        // Verifica se o dígito verificador da PRIMEIRA cartela é válido
        if (m_gameEngine.isValidCheckDigit(baseId, checkDigit)) {
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss");
            
            // Registra as N chances (ID, ID+1, ID+2...)
            for (int i = 0; i < numChances; ++i) {
                int currentId = baseId + i;
                if (!m_gameEngine.isTicketRegistered(currentId)) {
                    m_gameEngine.registerTicket(currentId);
                    m_saleTimestamps[currentId] = (numChances > 1) ? QString("%1 (Chance %2)").arg(timestamp).arg(i+1) : timestamp;
                    qInfo() << "Registrada cartela" << currentId << "(Chance" << (i+1) << ")";
                }
            }
            
            resp["action"] = "ticket_registered";
            resp["status"] = "ok";
            resp["ticketId"] = m_gameEngine.getFormattedBarcode(baseId);
            resp["timestamp"] = timestamp;
            resp["totalRegistered"] = m_gameEngine.getRegisteredCount();
            resp["chances"] = numChances;
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
    else if (action == "update_config") {
        if (json.contains("maxBalls")) m_gameEngine.setMaxBalls(json["maxBalls"].toInt());
        if (json.contains("gridIndex")) m_gameEngine.setGameMode(json["gridIndex"].toInt());
        if (json.contains("numChances")) m_gameEngine.setNumChances(json["numChances"].toInt());
        
        saveConfig();
        
        // Broadcast para todos ficarem sincronizados
        QJsonObject broadcast;
        broadcast["action"] = "config_updated";
        broadcast["maxBalls"] = m_gameEngine.getMaxBalls();
        broadcast["gridIndex"] = m_gameEngine.getGameMode();
        broadcast["numChances"] = m_gameEngine.getNumChances();
        broadcastJson(broadcast);
        
        qInfo() << "Configuração global atualizada pelo administrador";
    }
    else if (action == "register_random") {
        int count = json["count"].toInt();
        if (count <= 0) count = 10;
        
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss");
        
        const QVector<BingoTicket> &all = m_gameEngine.getAllTickets();
        if (!all.isEmpty()) {
            for (int i = 0; i < count; ++i) {
                int randomIndex = QRandomGenerator::global()->bounded(all.size());
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
