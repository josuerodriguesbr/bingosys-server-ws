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
    m_port(port),
    m_db(new BingoDatabaseManager(this)),
    m_historyLimit(10)
{
    // Conecta ao banco na inicialização (valores fixos conforme ambiente do usuário)
    if (m_db->connectToDatabase("localhost", "sorteios-bingo", "postgres", "")) {
        qInfo() << "BingoServer: Inicializado com PostgreSQL.";
    }
}

BingoServer::~BingoServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
    // Limpa instancias de jogo
    for(auto& inst : m_gameInstances) delete inst.engine;
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
    m_masterTickets = tickets;
    qInfo() << "BingoServer: Cartelas mestre carregadas:" << m_masterTickets.size();
}

BingoGameEngine* BingoServer::getEngine(int sorteioId)
{
    if (m_gameInstances.contains(sorteioId)) {
        return m_gameInstances[sorteioId].engine;
    }

    // Se não existe, cria um novo motor para este sorteio
    QJsonObject sorteio = m_db->getSorteio(sorteioId);
    if (sorteio.isEmpty()) return nullptr;

    GameInstance inst;
    inst.engine = new BingoGameEngine(this);
    inst.engine->loadTickets(m_masterTickets);
    inst.baseId = sorteio["base_id"].toInt();
    inst.modeloId = sorteio["modelo_id"].toInt();
    
    // Configura o motor com base nos dados do banco (ex: base_id)
    // Por enquanto usamos uma lógica simples de mapeamento
    inst.engine->setGameMode(inst.baseId - 1); // Exemplo de mapeamento
    
    // Carrega cartelas validadas para este jogo
    for (int ticketId : m_db->getCartelasValidadas(sorteioId)) {
        inst.engine->registerTicket(ticketId);
    }
    
    // Carrega bolas já sorteadas se o jogo já estiver em andamento
    for (int bola : m_db->getBolasSorteadas(sorteioId)) {
        inst.engine->processNumber(bola);
    }

    m_gameInstances.insert(sorteioId, inst);
    return inst.engine;
}

void BingoServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &BingoServer::processTextMessage);
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

void BingoServer::broadcastToGame(int sorteioId, const QJsonObject &json)
{
    QJsonDocument doc(json);
    QString msg = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().sorteioId == sorteioId) {
            it.key()->sendTextMessage(msg);
        }
    }
}

void BingoServer::handleJsonMessage(QWebSocket *client, const QJsonObject &json)
{
    QString action = json["action"].toString();
    
    if (action == "login") {
        QString chave = json["chave"].toString();
        QJsonObject res = m_db->validarChaveAcesso(chave);
        
        QJsonObject response;
        response["action"] = "login_response";
        
        if (!res.isEmpty()) {
            int sid = res["sorteio_id"].toInt();
            ClientSession session;
            session.sorteioId = sid;
            session.accessKey = chave;
            session.isOperator = (res["status"].toString() == "ativa"); // Chave valida = operador
            m_sessions[client] = session;

            response["status"] = "ok";
            response["sorteio_id"] = sid;
            response["is_operator"] = session.isOperator;
            
            // Sincroniza estado inicial do jogo
            BingoGameEngine *engine = getEngine(sid);
            if (engine) {
                response["totalRegistered"] = engine->getRegisteredCount();
                QJsonArray drawn;
                for(int n : engine->getDrawnNumbers()) drawn.append(n);
                response["drawnNumbers"] = drawn;
            }
        } else {
            response["status"] = "error";
            response["message"] = "Chave invalida ou ja utilizada";
        }
        sendJson(client, response);
        return;
    }

    if (action == "login_admin") {
        QString usuario = json["usuario"].toString();
        QString senha = json["senha"].toString();
        
        // LOGIN MESTRE HARDCODED (Conforme solicitado pelo usuário para primeiro acesso)
        // Recomenda-se mudar ou mover para config futuramente
        if (usuario == "admin" && senha == "Bingo2026!@#") {
            ClientSession session;
            session.sorteioId = 0; // 0 = Acesso Global
            session.isOperator = true;
            m_sessions[client] = session;

            QJsonObject response;
            response["action"] = "login_response";
            response["status"] = "ok";
            response["is_master"] = true;
            sendJson(client, response);
        } else {
            QJsonObject response;
            response["action"] = "login_response";
            response["status"] = "error";
            response["message"] = "Usuario ou senha mestre invalidos";
            sendJson(client, response);
        }
        return;
    }

    // Ações administrativas globais (is_master)
    if (m_sessions.contains(client) && m_sessions[client].sorteioId == 0) {
        if (action == "get_admin_data") {
            QJsonObject resp;
            resp["action"] = "admin_data_response";
            resp["sorteios"] = m_db->listarTodosSorteios();
            resp["chaves"] = m_db->listarTodasChavesAcesso();
            resp["modelos"] = m_db->listarModelos();
            resp["bases"] = m_db->listarBases();
            sendJson(client, resp);
            return;
        }

        if (action == "criar_chave") {
            int modeloId = json["modelo_id"].toInt();
            int baseId = json["base_id"].toInt();
            
            // Gera uma chave aleatória se não enviada
            QString chave = json["chave"].toString();
            if (chave.isEmpty()) {
                chave = "KEY-" + QString::number(QRandomGenerator::global()->bounded(10000, 99999)) + 
                        "-" + QString::number(QRandomGenerator::global()->bounded(1000, 9999));
            }

            int sid = m_db->criarSorteioComChave(modeloId, baseId, chave);
            
            QJsonObject resp;
            resp["action"] = "chave_criada_response";
            if (sid != -1) {
                resp["status"] = "ok";
                resp["sorteio_id"] = sid;
                resp["chave"] = chave;
                // Envia lista atualizada
                resp["sorteios"] = m_db->listarTodosSorteios();
                resp["chaves"] = m_db->listarTodasChavesAcesso();
            } else {
                resp["status"] = "error";
                resp["message"] = "Falha ao criar sorteio ou chave no banco de dados.";
            }
            sendJson(client, resp);
            return;
        }
    }

    // Ações que exigem estar logado em um sorteio
    if (!m_sessions.contains(client)) return;
    ClientSession &session = m_sessions[client];
    BingoGameEngine *engine = getEngine(session.sorteioId);
    if (!engine) return;

    if (action == "draw_number" && session.isOperator) {
        int number = json["number"].toInt();
        if (engine->processNumber(number)) {
            m_db->salvarBolaSorteada(session.sorteioId, number);
            
            QJsonObject broadcast;
            broadcast["action"] = "number_drawn";
            broadcast["number"] = number;
            
            // Winners e NearWins
            QJsonArray winnersArray;
            for(int w : engine->getWinners()) 
                winnersArray.append(getTicketDetailsJson(session.sorteioId, w));
            broadcast["winners"] = winnersArray;
            
            broadcastToGame(session.sorteioId, broadcast);
        }
    }
    else if (action == "register_ticket" && session.isOperator) {
        int barcode = json["barcode"].toInt();
        int ticketId = barcode / 10;
        int checkDigit = barcode % 10;
        
        if (engine->isValidCheckDigit(ticketId, checkDigit)) {
            if (m_db->registrarVenda(session.sorteioId, ticketId)) {
                engine->registerTicket(ticketId);
                
                QJsonObject resp;
                resp["action"] = "ticket_registered";
                resp["status"] = "ok";
                resp["ticketId"] = ticketId;
                resp["totalRegistered"] = engine->getRegisteredCount();
                broadcastToGame(session.sorteioId, resp);
            }
        }
    }
    else if (action == "get_my_tickets") {
        QString telefone = json["telefone"].toString();
        QList<int> ids = m_db->getCartelasPorTelefone(session.sorteioId, telefone);
        
        QJsonObject resp;
        resp["action"] = "my_tickets_response";
        QJsonArray ticketsArr;
        for (int id : ids) {
            ticketsArr.append(getTicketDetailsJson(session.sorteioId, id));
        }
        resp["tickets"] = ticketsArr;
        sendJson(client, resp);
    }
}

void BingoServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        qInfo() << "Cliente desconectado:" << pClient->peerAddress().toString();
        m_clients.removeAll(pClient);
        m_sessions.remove(pClient);
        pClient->deleteLater();
    }
}

QJsonObject BingoServer::getTicketDetailsJson(int sorteioId, int ticketId)
{
    Q_UNUSED(sorteioId); // Por enquanto as cartelas sao globais m_masterTickets
    QJsonObject obj;
    BingoGameEngine *engine = getEngine(sorteioId);
    if (!engine) return obj;

    obj["barcode"] = engine->getFormattedBarcode(ticketId);
    obj["ticketId"] = ticketId;
    
    QJsonArray nums;
    for(int n : engine->getTicketNumbers(ticketId)) {
        nums.append(n);
    }
    obj["numbers"] = nums;
    
    return obj;
}
