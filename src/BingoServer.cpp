#include "BingoServer.h"
#include "BingoTicketParser.h"
#include <algorithm>
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
    if (m_db->connectToDatabase("localhost", "bingosys", "bingosys", "bingosys")) {
        qInfo() << "BingoServer: Inicializado com PostgreSQL Local.";
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


BingoGameEngine* BingoServer::getEngine(int sorteioId)
{
    if (m_gameInstances.contains(sorteioId)) {
        return m_gameInstances[sorteioId].engine;
    }

    // Se não existe, cria um novo motor para este sorteio
    QJsonObject sorteio = m_db->getSorteio(sorteioId);
    if (sorteio.isEmpty()) return nullptr;

    QString dataPath = sorteio["caminho_dados"].toString();
    if (dataPath.isEmpty()) {
        qCritical() << "BingoServer: Caminho de dados não definido para o sorteio" << sorteioId;
        return nullptr;
    }

    // Cache de Cartelas
    if (!m_ticketCache.contains(dataPath)) {
        qInfo() << "BingoServer: Carregando nova base de cartelas:" << dataPath;
        auto tickets = BingoTicketParser::parseFile(dataPath);
        if (tickets.isEmpty()) {
            qCritical() << "BingoServer: Falha crítica ao carregar base:" << dataPath;
            return nullptr;
        }
        m_ticketCache.insert(dataPath, tickets);
    }
    const QVector<BingoTicket> &activeTickets = m_ticketCache[dataPath];

    GameInstance inst;
    inst.engine = new BingoGameEngine(this);
    inst.engine->loadTickets(activeTickets);
    inst.baseId = sorteio["base_id"].toInt();
    inst.modeloId = sorteio["modelo_id"].toInt();
    
    // Configura o motor com base no tipo de grade da base
    QString tipoGrade = sorteio["tipo_grade"].toString(); // Ex: "75x25", "75x15"
    int nNums = 15; // default
    if (tipoGrade.contains('x')) nNums = tipoGrade.split('x').last().toInt();

    int gridIdx = 0;
    if (!activeTickets.isEmpty()) {
        const auto &grids = activeTickets.first().grids;
        for (int i = 0; i < grids.size(); ++i) {
            if (grids[i].size() == nNums) {
                gridIdx = i;
                break;
            }
        }
    }
    inst.engine->setGameMode(gridIdx);
    
    qInfo() << "BingoServer: Sorteio" << sorteioId << "iniciado usando" << dataPath << "Grade:" << gridIdx << "(" << nNums << "números )";

    // Carrega cartelas validadas para este jogo
    for (int ticketId : m_db->getCartelasValidadas(sorteioId)) {
        inst.engine->registerTicket(ticketId);
    }
    
    // Carrega premiações
    QJsonArray prizes = m_db->getPremiacoes(sorteioId);
    for (int i = 0; i < prizes.size(); ++i) {
        QJsonObject pObj = prizes[i].toObject();
        Prize p;
        p.id = pObj["id"].toInt();
        p.nome = pObj["nome"].toString();
        p.tipo = pObj["tipo"].toString();
        p.active = true;
        p.realizada = pObj["realizada"].toBool();
        
        QJsonArray padrao = pObj["padrao"].toArray();
        for (const QJsonValue &v : padrao) p.padraoIndices.insert(v.toInt());
        
        inst.engine->addPrize(p);
    }

    // Carrega bolas já sorteadas
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
    
    int count = 0;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().sorteioId == sorteioId) {
            it.key()->sendTextMessage(msg);
            count++;
        }
    }
    qInfo() << "Broadcast p/ Sorteio" << sorteioId << "Evento:" << json["action"].toString() << "Enviado p/" << count << "clientes.";
}

void BingoServer::handleJsonMessage(QWebSocket *client, const QJsonObject &json)
{
    QString action = json["action"].toString();
    qInfo() << "Mensagem Recebida - Açao:" << action << "Client:" << client->peerAddress().toString();
    if (action == "ping") {
        QJsonObject pong;
        pong["action"] = "pong";
        sendJson(client, pong);
        return;
    }

    if (action == "login") {
        QString chave = json["chave"].toString();
        
        // Verifica se é o Token Mestre atual
        if (!m_masterToken.isEmpty() && chave == m_masterToken) {
            ClientSession session;
            session.sorteioId = 0;
            session.isOperator = true;
            m_sessions[client] = session;

            QJsonObject response;
            response["action"] = "login_response";
            response["status"] = "ok";
            response["is_master"] = true;
            response["sorteio_id"] = 0;
            sendJson(client, response);
            return;
        }

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
                QJsonObject sync = getGameStatusJson(sid);
                sync["action"] = "sync_status";
                sendJson(client, sync);
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

            // Gera um token de sessão mestre para persistência (refresh de página)
            m_masterToken = "MASTER-" + QString::number(QRandomGenerator::global()->generate()).mid(0, 8);

            QJsonObject response;
            response["action"] = "login_response";
            response["status"] = "ok";
            response["is_master"] = true;
            response["token"] = m_masterToken;
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

        if (action == "update_config") {
            int sid = json["sorteio_id"].toInt();
            int modeloId = json["modelo_id"].toInt();
            int baseId = json["base_id"].toInt();
            QJsonObject prefs = json["preferencias"].toObject();

            if (m_db->atualizarConfigSorteio(sid, modeloId, baseId, prefs)) {
                QJsonObject sorteio = m_db->getSorteio(sid);
                QString tipoG = sorteio["tipo_grade"].toString();

                if (m_gameInstances.contains(sid)) {
                    auto *engine = m_gameInstances[sid].engine;
                    if (!m_ticketCache.contains(tipoG)) { // Simplificação: tipoG ou caminho? Melhor buscar o caminho.
                        QJsonObject st = m_db->getSorteio(sid);
                        QString path = st["caminho_dados"].toString();
                        if (!m_ticketCache.contains(path)) {
                            auto tks = BingoTicketParser::parseFile(path);
                            if (!tks.isEmpty()) m_ticketCache.insert(path, tks);
                        }
                        if (m_ticketCache.contains(path)) {
                            const auto &activeTks = m_ticketCache[path];
                            engine->loadTickets(activeTks);
                            int nNums = (tipoG.contains('x')) ? tipoG.split('x').last().toInt() : 15;
                            int gridIdx = 0;
                            if (!activeTks.isEmpty()) {
                                const auto &grids = activeTks.first().grids;
                                for (int i = 0; i < grids.size(); ++i) {
                                    if (grids[i].size() == nNums) { gridIdx = i; break; }
                                }
                            }
                            engine->setGameMode(gridIdx);
                        }
                    }
                }

                QJsonObject resp;
                resp["action"] = "config_updated";
                resp["status"] = "ok";
                resp["sorteio_id"] = sid;
                resp["tipo_grade"] = tipoG;
                sendJson(client, resp);
                broadcastToGame(sid, resp);
            }
            return;
        }

        if (action == "save_as_template") {
            QString nome = json["nome"].toString();
            QJsonObject config = json["config"].toObject();
            if (m_db->salvarSorteioComoModelo(nome, config)) {
                QJsonObject resp;
                resp["action"] = "template_saved";
                resp["status"] = "ok";
                sendJson(client, resp);
            }
            return;
        }
    }

    // Ações que exigem estar logado em um sorteio
    if (!m_sessions.contains(client)) {
        qWarning() << "Tentativa de acao sem login:" << action;
        return;
    }
    ClientSession &session = m_sessions[client];
    qInfo() << "Açao de Jogo:" << action << "SorteioID:" << session.sorteioId << "IsOperator:" << session.isOperator;

    // --- AÇÕES QUE NÃO EXIGEM MOTOR (ENGINE) CARREGADO ---
    
    // Handler get_draw_config - usa session.sorteioId
    if (action == "get_draw_config") {
        QJsonObject resp;
        resp["action"] = "draw_config_response";
        resp["draw"] = m_db->getSorteio(session.sorteioId);
        resp["modelos"] = m_db->listarModelos();
        resp["bases"] = m_db->listarBases();
        sendJson(client, resp);
        return;
    }

    if (action == "get_prizes") {
        QJsonObject resp;
        resp["action"] = "prizes_list";
        resp["prizes"] = m_db->getPremiacoes(session.sorteioId);
        sendJson(client, resp);
        return;
    }

    if (action == "get_my_tickets") {
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
        return;
    }

    if (action == "update_config" && session.isOperator) {
        int modeloId = json["modelo_id"].toInt();
        int baseId = json["base_id"].toInt();
        QJsonObject prefs = json["preferencias"].toObject();

        qInfo() << "[DEBUG] update_config recebido: sorteioId=" << session.sorteioId 
                << "modeloId=" << modeloId << "baseId=" << baseId;

        if (m_db->atualizarConfigSorteio(session.sorteioId, modeloId, baseId, prefs)) {
            qInfo() << "[DEBUG] m_db->atualizarConfigSorteio retornou TRUE";
            QJsonObject sorteio = m_db->getSorteio(session.sorteioId);
            QString tipoG = sorteio["tipo_grade"].toString();
            QString dataPath = sorteio["caminho_dados"].toString();

            // Sincroniza o motor (engine) se ele já existir ou força criação se a base for válida agora
            BingoGameEngine *engine = getEngine(session.sorteioId);
            if (engine) {
                // Se a base mudou, recarrega cartelas
                if (!m_ticketCache.contains(dataPath)) {
                    qInfo() << "BingoServer: Carregando nova base via update_config:" << dataPath;
                    auto tickets = BingoTicketParser::parseFile(dataPath);
                    if (!tickets.isEmpty()) m_ticketCache.insert(dataPath, tickets);
                }
                
                if (m_ticketCache.contains(dataPath)) {
                    const auto &activeTickets = m_ticketCache[dataPath];
                    engine->loadTickets(activeTickets);
                    
                    int nNums = (tipoG.contains('x')) ? tipoG.split('x').last().toInt() : 15;
                    int gridIdx = 0;
                    if (!activeTickets.isEmpty()) {
                        const auto &grids = activeTickets.first().grids;
                        for (int i = 0; i < grids.size(); ++i) {
                            if (grids[i].size() == nNums) { gridIdx = i; break; }
                        }
                    }
                    engine->setGameMode(gridIdx);
                    qInfo() << "BingoServer: Base do Sorteio" << session.sorteioId << "atualizada para" << dataPath << "Grade:" << gridIdx;
                }
            }

            // Envia resposta de OK para o cliente que solicitou
            QJsonObject resp;
            resp["action"] = "config_updated";
            resp["status"] = "ok";
            resp["maxBalls"] = (tipoG.startsWith("90") ? 90 : 75); // Estimativa simples
            sendJson(client, resp);

            // Broadcast de sync para todos os participantes do sorteio (atualiza o modo visual)
            QJsonObject sync = getGameStatusJson(session.sorteioId);
            sync["action"] = "sync_status";
            broadcastToGame(session.sorteioId, sync);
        }
        return;
    }

    // --- AÇÕES QUE EXIGEM MOTOR (ENGINE) CARREGADO ---
    BingoGameEngine *engine = getEngine(session.sorteioId);
    if (!engine) {
        qWarning() << "BingoServer: Falha ao carregar motor para sorteio" << session.sorteioId << ". Verifique a base de dados.";
        return;
    }

    if (action == "draw_number" && session.isOperator) {
        // Verifica se há prêmios pendentes
        bool temPremioPendente = false;
        auto prizes = engine->getPrizes();
        for(const auto &p : prizes) {
            if (p.active && !p.realizada) {
                temPremioPendente = true;
                break;
            }
        }

        if (!temPremioPendente) {
            // Tenta sincronizar prêmios do banco um última vez antes de falhar
            qInfo() << "BingoServer: Nenhuma premiacao pendente no engine. Recarregando do banco para o sorteio" << session.sorteioId;
            QJsonArray dbPrizes = m_db->getPremiacoes(session.sorteioId);
            engine->clearPrizes();
            for(int i = 0; i < dbPrizes.size(); ++i) {
                QJsonObject pObj = dbPrizes[i].toObject();
                Prize p;
                p.id = pObj["id"].toInt();
                p.nome = pObj["nome"].toString();
                p.tipo = pObj["tipo"].toString();
                p.active = true;
                p.realizada = pObj["realizada"].toBool();
                QJsonArray padrao = pObj["padrao"].toArray();
                for (const QJsonValue &v : padrao) p.padraoIndices.insert(v.toInt());
                engine->addPrize(p);
                if (p.active && !p.realizada) temPremioPendente = true;
            }
        }

        if (!temPremioPendente) {
            QString statusMsg = "Não existem premiações ativas pendentes.";
            auto listP = engine->getPrizes();
            if (listP.isEmpty()) {
                statusMsg += " Nenhuma premiação foi cadastrada para este sorteio.";
            } else {
                statusMsg += " Prêmios atuais: ";
                for(const auto &p : listP) {
                    statusMsg += QString("[%1: %2] ").arg(p.nome).arg(p.realizada ? "REALIZADO" : "PENDENTE");
                }
            }
            
            QJsonObject error;
            error["action"] = "draw_number_error";
            error["message"] = statusMsg;
            sendJson(client, error);
            qWarning() << "BingoServer: Bloqueio de sorteio para SorteioID" << session.sorteioId << "-" << statusMsg;
            return;
        }

        int number = json["number"].toInt();
        engine->processNumber(number); 
        m_db->salvarBolaSorteada(session.sorteioId, number);
        
        // --- AUTOMAÇÃO: Verifica se algum prêmio foi ganho agora ---
        prizes = engine->getPrizes();
        QList<Prize*> winningPrizes;
        
        // Identifica prêmios que NÃO estavam realizados mas AGORA tem ganhadores
        for(int i = 0; i < prizes.size(); ++i) {
            if (prizes[i].active && !prizes[i].realizada && !prizes[i].winners.isEmpty()) {
                int pid = prizes[i].id;
                m_db->atualizarStatusPremiacao(pid, true);
                engine->setPrizeStatus(pid, true);
                qInfo() << "BingoServer: Prêmio" << pid << "(" << prizes[i].nome << ") marcado automaticamente como REALIZADO.";
            }
        }
        
        QJsonObject broadcast = getGameStatusJson(session.sorteioId);
        broadcast["action"] = "number_drawn";
        broadcast["number"] = number;
        
        broadcastToGame(session.sorteioId, broadcast);
    }
    else if (action == "finalize_prize" && session.isOperator) {
        int prizeId = json["prizeId"].toInt();
        bool realizada = json["realizada"].toBool(true);

        if (m_db->atualizarStatusPremiacao(prizeId, realizada)) {
             // Atualiza no motor vivo
             auto prizes = engine->getPrizes();
             engine->clearPrizes();
             for(auto &p : prizes) {
                 if (p.id == prizeId) p.realizada = realizada;
                 engine->addPrize(p);
             }

             QJsonObject resp;
             resp["action"] = "prize_status_updated";
             resp["prizeId"] = prizeId;
             resp["realizada"] = realizada;
             broadcastToGame(session.sorteioId, resp);

             // Envia um sync completo para garantir que todos vejam o status novo
             QJsonObject sync = getGameStatusJson(session.sorteioId);
             sync["action"] = "sync_status";
             broadcastToGame(session.sorteioId, sync);
        }
    }
    else if (action == "undo_last" && session.isOperator) {
        int num = engine->undoLastNumber();
        if (num != -1) {
            m_db->removerUltimaBola(session.sorteioId, num);
            QJsonObject broadcast = getGameStatusJson(session.sorteioId);
            broadcast["action"] = "number_cancelled";
            broadcast["number"] = num;
            broadcastToGame(session.sorteioId, broadcast);
        }
    }
    else if (action == "start_game" && session.isOperator) {
        engine->startNewGame();
        m_db->limparSorteio(session.sorteioId);
        QJsonObject broadcast;
        broadcast["action"] = "game_started";
        broadcastToGame(session.sorteioId, broadcast);
        
        // Envia sync completo logo após reset
        QJsonObject sync = getGameStatusJson(session.sorteioId);
        sync["action"] = "sync_status";
        broadcastToGame(session.sorteioId, sync);
    }
    else if (action == "get_prizes") {
        // Já tratado acima
    }
    else if (action == "add_prize" && session.isOperator) {
        QString nome = json["nome"].toString();
        QString tipo = json["tipo"].toString();
        QJsonArray padrao = json["padrao"].toArray();
        int ordem = json["ordem"].toInt();
        
        if (m_db->addPremiacao(session.sorteioId, nome, tipo, padrao, ordem)) {
            // Recarrega o motor para refletir a mudança (ou apenas adiciona ao motor vivo)
            Prize p;
            p.nome = nome;
            p.tipo = tipo;
            p.active = true;
            for(const QJsonValue &v : padrao) p.padraoIndices.insert(v.toInt());
            engine->addPrize(p);

            QJsonObject resp;
            resp["action"] = "prize_added";
            resp["status"] = "ok";
            sendJson(client, resp);
            
            // Sync status para todos
            QJsonObject sync = getGameStatusJson(session.sorteioId);
            sync["action"] = "sync_status";
            broadcastToGame(session.sorteioId, sync);
        }
    }
    else if (action == "delete_prize" && session.isOperator) {
        int prizeId = json["id"].toInt();
        if (m_db->removerPremiacao(prizeId)) {
            // Para simplificar, poderíamos recarregar o motor ou apenas avisar
            // Mas o prêmio ID no motor pode ser diferente. Melhor forçar reload ou sync.
            // Para segurança total em tempo real:
            engine->clearPrizes();
            QJsonArray dbPrizes = m_db->getPremiacoes(session.sorteioId);
            for(int i=0; i<dbPrizes.size(); ++i) {
                QJsonObject po = dbPrizes[i].toObject();
                Prize p;
                p.id = po["id"].toInt();
                p.nome = po["nome"].toString();
                p.tipo = po["tipo"].toString();
                p.active = true;
                QJsonArray pad = po["padrao"].toArray();
                for(const QJsonValue &v : pad) p.padraoIndices.insert(v.toInt());
                engine->addPrize(p);
            }

            QJsonObject resp;
            resp["action"] = "prize_deleted";
            resp["id"] = prizeId;
            broadcastToGame(session.sorteioId, resp);
            
            QJsonObject sync = getGameStatusJson(session.sorteioId);
            sync["action"] = "sync_status";
            broadcastToGame(session.sorteioId, sync);
        }
    }
    else if (action == "register_ticket" && session.isOperator) {
        int barcode = json["barcode"].toInt();
        QString telefone = json["telefone"].toString();
        int ticketId = barcode / 10;
        int checkDigit = barcode % 10;
        
        if (engine->isValidCheckDigit(ticketId, checkDigit)) {
            if (m_db->registrarVenda(session.sorteioId, ticketId, telefone)) {
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
    else if (action == "register_random" && session.isOperator) {
        int count = json["count"].toInt(100);
        QString telefone = json["telefone"].toString();
        int registered = 0;
        
        const QVector<BingoTicket>& all = engine->getAllTickets();
        QVector<int> availableIds;
        for(const auto& t : all) {
            if (!engine->isTicketRegistered(t.id)) {
                availableIds.append(t.id);
            }
        }

        // Embaralha para pegar aleatórios
        std::shuffle(availableIds.begin(), availableIds.end(), *QRandomGenerator::global());

        int limit = qMin(count, availableIds.size());
        for(int i = 0; i < limit; ++i) {
            int tid = availableIds[i];
            if (m_db->registrarVenda(session.sorteioId, tid, telefone, "Teste")) {
                engine->registerTicket(tid);
                registered++;
            }
        }

        QJsonObject resp;
        resp["action"] = "batch_registration_finished";
        resp["count"] = registered;
        resp["totalRegistered"] = engine->getRegisteredCount();
        sendJson(client, resp);
        broadcastToGame(session.sorteioId, resp);
    }
    else if (action == "import_sales" && session.isOperator) {
        QJsonArray list = json["list"].toArray();
        int count = 0;
        for(int i = 0; i < list.size(); ++i) {
            QJsonObject item = list[i].toObject();
            int barcode = item["barcode"].toInt();
            QString tel = item["telefone"].toString();
            int tid = barcode / 10;
            int check = barcode % 10;

            if (engine->isValidCheckDigit(tid, check) && !engine->isTicketRegistered(tid)) {
                if (m_db->registrarVenda(session.sorteioId, tid, tel, "Importacao")) {
                    engine->registerTicket(tid);
                    count++;
                }
            }
        }
        QJsonObject resp;
        resp["action"] = "batch_registration_finished";
        resp["count"] = count;
        resp["totalRegistered"] = engine->getRegisteredCount();
        sendJson(client, resp);
        broadcastToGame(session.sorteioId, resp);
    }
    else if (action == "get_my_tickets") {
        // Já tratado acima
    }
    else if (action == "update_config" && session.isOperator) {
        // Já tratado acima
    }
    else if (action == "save_as_template") {
        QString nome = json["nome"].toString();
        QJsonObject config = json["config"].toObject();
        if (m_db->salvarSorteioComoModelo(nome, config)) {
            QJsonObject resp;
            resp["action"] = "template_saved";
            resp["status"] = "ok";
            sendJson(client, resp);
        }
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


QJsonObject BingoServer::getGameStatusJson(int sorteioId)
{
    QJsonObject sync;
    BingoGameEngine *engine = getEngine(sorteioId);
    if (!engine) return sync;

    sync["totalRegistered"] = engine->getRegisteredCount();
    
    QJsonArray drawn;
    for(int n : engine->getDrawnNumbers()) drawn.append(n);
    sync["drawnNumbers"] = drawn;
    
    // Winners
    QJsonArray winnersArray;
    for(int w : engine->getWinners()) 
        winnersArray.append(getTicketDetailsJson(sorteioId, w));
    sync["winners"] = winnersArray;

    // Near Wins (Boas)
    QJsonObject nearWins;
    auto nwMap = engine->getNearWinTickets();
    for(auto it = nwMap.begin(); it != nwMap.end(); ++it) {
        QJsonArray arr;
        int count = 0;
        for(int id : it.value()) {
            arr.append(getTicketDetailsJson(sorteioId, id));
            if (++count >= 10) break;
        }
        nearWins[QString::number(it.key())] = arr;
    }
    sync["near_wins"] = nearWins;
    
    // Premiações Customizadas
    QJsonArray prizesArray;
    for(const auto &p : engine->getPrizes()) {
        QJsonObject po;
        po["id"] = p.id;
        po["nome"] = p.nome;
        po["tipo"] = p.tipo;
        po["realizada"] = p.realizada;
        po["active"] = p.active;
        QJsonArray pWinners;
        for(int id : p.winners) pWinners.append(getTicketDetailsJson(sorteioId, id));
        po["winners"] = pWinners;

        QJsonArray pNearWinners;
        for(int id : p.near_winners) pNearWinners.append(getTicketDetailsJson(sorteioId, id));
        po["near_winners"] = pNearWinners;

        QJsonArray padraoArr;
        for(int idx : p.padraoIndices) padraoArr.append(idx);
        po["padrao"] = padraoArr;

        prizesArray.append(po);
    }
    sync["prizes"] = prizesArray;

    QJsonObject sorteio = m_db->getSorteio(sorteioId);
    QJsonObject prefs = sorteio["preferencias"].toObject();
    sync["historyLimit"] = prefs["historyLimit"].toInt(10);
    sync["numChances"] = prefs["numChances"].toInt(1);
    sync["maxBalls"] = 75; // Todo: obter do banco se disponível
    sync["tipo_grade"] = sorteio["tipo_grade"].toString();

    return sync;
}
