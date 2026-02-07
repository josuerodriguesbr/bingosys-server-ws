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

    QJsonObject sorteio = m_db->getSorteio(sorteioId);
    if (sorteio.isEmpty()) return nullptr;

    QJsonArray rodadasArr = m_db->getRodadas(sorteioId);
    if (rodadasArr.isEmpty()) {
        qWarning() << "BingoServer: Sorteio" << sorteioId << "não possui rodadas cadastradas.";
    }

    GameInstance inst;
    inst.engine = new BingoGameEngine(this);
    inst.modeloId = sorteio["modelo_id"].toInt();
    
    // Carrega todas as bases requeridas pelas rodadas
    QSet<int> requiredBases;
    for (int i = 0; i < rodadasArr.size(); ++i) {
        requiredBases.insert(rodadasArr[i].toObject()["base_id"].toInt());
    }

    for (int bid : requiredBases) {
        // Precisamos do caminho_dados desta base. 
        // Como o getRodadas já retornou o caminho_dados para cada rodada (via JOIN), 
        // vamos pegá-lo da primeira rodada que usa esta base.
        QString dataPath;
        for (int i = 0; i < rodadasArr.size(); ++i) {
            QJsonObject rObj = rodadasArr[i].toObject();
            if (rObj["base_id"].toInt() == bid) {
                dataPath = rObj["caminho_dados"].toString();
                break;
            }
        }

        if (dataPath.isEmpty()) {
            qWarning() << "BingoServer: Base" << bid << "não possui caminho de dados válido.";
            continue;
        }

        if (!m_ticketCache.contains(dataPath)) {
            qInfo() << "BingoServer: Carregando base de cartelas:" << dataPath;
            auto tickets = BingoTicketParser::parseFile(dataPath);
            if (!tickets.isEmpty()) m_ticketCache.insert(dataPath, tickets);
        }

        if (m_ticketCache.contains(dataPath)) {
            inst.engine->loadBase(bid, m_ticketCache[dataPath]);
        }
    }

    // Carrega cartelas validadas
    for (int ticketId : m_db->getCartelasValidadas(sorteioId)) {
        inst.engine->registerTicket(ticketId);
    }
    
    // Adiciona prêmios ao motor
    for (int i = 0; i < rodadasArr.size(); ++i) {
        QJsonObject rodadaObj = rodadasArr[i].toObject();
        int baseId = rodadaObj["base_id"].toInt();
        QString tipoGrade = rodadaObj["tipo_grade"].toString();
        QString caminhoDados = rodadaObj["caminho_dados"].toString();
        QJsonObject configuracoesRodada = rodadaObj["configuracoes"].toObject();

        // Determina o gridIndex com base no tipo_grade (Ex: "75x25" -> 25)
        int nNums = 15;
        if (tipoGrade.contains('x')) nNums = tipoGrade.split('x').last().toInt();
        
        int gridIdx = 0;
        const auto &baseTickets = m_ticketCache[caminhoDados];
        if (!baseTickets.isEmpty()) {
            const auto &grids = baseTickets.first().grids;
            for (int j = 0; j < grids.size(); ++j) {
                if (grids[j].size() == nNums) { gridIdx = j; break; }
            }
        }

        // Agora injetamos cada PREMIO como uma regra independente no motor
        QJsonArray premiosArr = rodadaObj["premios"].toArray();
        for (int j = 0; j < premiosArr.size(); ++j) {
            QJsonObject premioObj = premiosArr[j].toObject();
            Prize p;
            p.id = premioObj["id"].toInt(); // Usamos o ID do prêmio para controle de vitória
            p.nome = QString("%1 (%2)").arg(rodadaObj["nome"].toString(), premioObj["tipo"].toString().toUpper());
            p.tipo = premioObj["tipo"].toString();
            p.baseId = baseId;
            p.gridIndex = gridIdx;
            p.active = true;
            p.realizada = premioObj["realizada"].toBool();
            p.configuracoes = configuracoesRodada; // Herda configurações da rodada (ex: chances)

            QJsonArray padrao = premioObj["padrao"].toArray();
            for (const QJsonValue &v : padrao) p.padraoIndices.insert(v.toInt());
            
            inst.engine->addPrize(p);
        }
    }

    // Inicializa o modo de jogo (isso vai criar os TicketStates com os gridIndices corretos)
    inst.engine->setGameMode(0); 

    // Carrega bolas sorteadas
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
            session.chaveId = res["id"].toInt();
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
            json["base_id"].toInt();
            
            // Gera uma chave aleatória se não enviada
            QString chave = json["chave"].toString();
            if (chave.isEmpty()) {
                chave = "KEY-" + QString::number(QRandomGenerator::global()->bounded(10000, 99999)) + 
                        "-" + QString::number(QRandomGenerator::global()->bounded(1000, 9999));
            }

            int sid = m_db->criarSorteioComChave(modeloId, chave);
            
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
            // base_id e preferencias agora são por prêmio, mas aceitamos se enviados globalmente para legado
            
            if (m_db->atualizarConfigSorteio(sid, modeloId)) {
                // Se houver campos de agendamento, atualiza
                if (json.contains("data")) {
                    QDate d = QDate::fromString(json["data"].toString(), Qt::ISODate);
                    QTime h1 = QTime::fromString(json["hora_inicio"].toString(), Qt::ISODate);
                    QTime h2 = QTime::fromString(json["hora_fim"].toString(), Qt::ISODate);
                    m_db->atualizarAgendamentoSorteio(sid, d, h1, h2);
                }

                QJsonObject resp;
                resp["action"] = "config_updated";
                resp["status"] = "ok";
                resp["sorteio_id"] = sid;
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
        QJsonObject sorteio = m_db->getSorteio(session.sorteioId);
        
        // Injeta tipo_grade do primeiro prêmio ou default para o frontend
        QJsonArray rodadas = m_db->getRodadas(session.sorteioId);
        if (!rodadas.isEmpty()) {
            sorteio["tipo_grade"] = rodadas[0].toObject()["tipo_grade"].toString();
        } else {
            sorteio["tipo_grade"] = "75x15";
        }

        resp["draw"] = sorteio;
        resp["modelos"] = m_db->listarModelos();
        resp["bases"] = m_db->listarBases();
        sendJson(client, resp);
        return;
    }

    if (action == "get_rodadas") {
        QJsonObject resp;
        resp["action"] = "rodadas_list";
        resp["rodadas"] = m_db->getRodadas(session.sorteioId);
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
        
        if (m_db->atualizarConfigSorteio(session.sorteioId, modeloId)) {
            // Agendamento
            if (json.contains("data")) {
                QDate d = QDate::fromString(json["data"].toString(), Qt::ISODate);
                QTime h1 = QTime::fromString(json["hora_inicio"].toString(), Qt::ISODate);
                QTime h2 = QTime::fromString(json["hora_fim"].toString(), Qt::ISODate);
                m_db->atualizarAgendamentoSorteio(session.sorteioId, d, h1, h2);
            }

            // Envia resposta de OK
            QJsonObject resp;
            resp["action"] = "config_updated";
            resp["status"] = "ok";
            sendJson(client, resp);

            // Força recarga do motor se necessário (pode mudar prêmios/bases)
            if (m_gameInstances.contains(session.sorteioId)) {
                delete m_gameInstances[session.sorteioId].engine;
                m_gameInstances.remove(session.sorteioId);
            }
            getEngine(session.sorteioId);

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
        // REGRA CRÍTICA: Se o sorteio já terminou, não permite novos números de jeito nenhum.
        // Isso evita que números "fantasmas" entrem no cache e corrompam o 'Desfazer'.
        QJsonObject currentStatus = getGameStatusJson(session.sorteioId);
        if (currentStatus["isFinished"].toBool()) {
             QJsonObject error;
             error["action"] = "draw_number_error";
             error["message"] = "Sorteio já concluído. Não é possível inserir mais números.";
             sendJson(client, error);
             qWarning() << "[SECURITY] Tentativa de draw_number em sorteio concluído. SorteioID:" << session.sorteioId;
             return;
        }

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
            // Tenta sincronizar rodadas/prêmios do banco um última vez antes de falhar
            qInfo() << "BingoServer: Nenhuma premiação pendente no engine. Recarregando do banco para o sorteio" << session.sorteioId;
            QJsonArray dbRodadas = m_db->getRodadas(session.sorteioId);
            engine->clearPrizes();
            for(int i = 0; i < dbRodadas.size(); ++i) {
                QJsonObject rObj = dbRodadas[i].toObject();
                QJsonArray premios = rObj["premios"].toArray();
                for(int j = 0; j < premios.size(); ++j) {
                    QJsonObject pObj = premios[j].toObject();
                    Prize p;
                    p.id = pObj["id"].toInt();
                    p.nome = QString("%1 (%2)").arg(rObj["nome"].toString(), pObj["tipo"].toString().toUpper());
                    p.tipo = pObj["tipo"].toString();
                    p.active = true;
                    p.realizada = pObj["realizada"].toBool();
                    QJsonArray padrao = pObj["padrao"].toArray();
                    for (const QJsonValue &v : padrao) p.padraoIndices.insert(v.toInt());
                    engine->addPrize(p);
                    if (p.active && !p.realizada) temPremioPendente = true;
                }
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
                m_db->atualizarStatusPremio(pid, true);
                engine->setPrizeStatus(pid, true);
                qInfo() << "BingoServer: Prêmio" << pid << "(" << prizes[i].nome << ") marcado automaticamente como REALIZADO.";
            }
        }
        
        QJsonObject broadcast = getGameStatusJson(session.sorteioId);
        broadcast["action"] = "number_drawn";
        broadcast["number"] = number;
        
        broadcastToGame(session.sorteioId, broadcast);

        // Se o sorteio terminou, bloqueia a chave que iniciou o processo (se for operador)
        if (broadcast["isFinished"].toBool() && session.isOperator && session.chaveId > 0) {
            m_db->bloquearChave(session.chaveId);
            qInfo() << "[SECURITY] Sorteio" << session.sorteioId << "concluído. Chave" << session.chaveId << "inativada.";
        }
    }
    else if (action == "finalize_prize" && session.isOperator) {
        int premioId = json["prizeId"].toInt();
        bool realizada = json["realizada"].toBool(true);

        if (m_db->atualizarStatusPremio(premioId, realizada)) {
             // Atualiza no motor vivo diretamente
             engine->setPrizeStatus(premioId, realizada);

             QJsonObject sync = getGameStatusJson(session.sorteioId);
             
             QJsonObject resp;
             resp["action"] = "premio_status_updated";
             resp["prizeId"] = premioId;
             resp["realizada"] = realizada;
             resp["isFinished"] = sync["isFinished"]; // Redundância de segurança
             broadcastToGame(session.sorteioId, resp);

             sync["action"] = "sync_status";
             broadcastToGame(session.sorteioId, sync);

             // REGRA DE SEGURANÇA: Bloqueio/Reativação de Chave
             bool isFinished = sync["isFinished"].toBool();
             if (session.isOperator && session.chaveId > 0) {
                 if (isFinished) {
                     m_db->bloquearChave(session.chaveId);
                     qInfo() << "[SECURITY] Sorteio" << session.sorteioId << "concluído (manual). Chave" << session.chaveId << "inativada.";
                 } else if (!realizada) {
                     // Se reabriu um prêmio, garante que a chave volte a ser ATIVA
                     m_db->reativarChave(session.chaveId);
                     qInfo() << "[SECURITY] Sorteio" << session.sorteioId << "REABERTO (manual). Chave" << session.chaveId << "reativada.";
                 }
             }
        }
    }
    else if (action == "undo_last" && session.isOperator) {
        // 1. Captura o estado dos prêmios ANTES do undo para saber quais reabrir (se necessário)
        QSet<int> preRealizedIds;
        auto prizesBefore = engine->getPrizes();
        for (const auto &p : prizesBefore) {
            if (p.realizada) preRealizedIds.insert(p.id);
        }

        // 2. Executa o undo no motor passando os IDs que já estavam realizados
        int num = engine->undoLastNumber(preRealizedIds);

        if (num != -1) {
            bool dbOk = m_db->removerUltimaBola(session.sorteioId, num);
            qInfo() << "[UNDO] Bola" << num << "removida. DB status:" << dbOk;
            
            // 3. RE-AVALIAÇÃO DE REABERTURA:
            // Sincronizamos o status no DB apenas para prêmios que estavam realizados mas agora não estão.
            auto prizesAfter = engine->getPrizes();
            int reabertos = 0;
            QString reabertosNomes;

            for(const auto &p : prizesAfter) {
                // Se o prêmio estava realizado ANTES, mas no replay sem a última bola o motor diz que NÃO está 'realizada'
                // (isso acontece porque ele perdeu os ganhadores com a remoção da bola).
                if (preRealizedIds.contains(p.id) && !p.realizada) {
                    m_db->atualizarStatusPremio(p.id, false);
                    reabertos++;
                    reabertosNomes += p.nome + " ";
                }
            }

            if (reabertos > 0) {
                qInfo() << "[UNDO-REOPEN] Sorteio" << session.sorteioId << ". Prêmios reabertos automaticamente:" << reabertosNomes;
            }

            // Força o motor a processar o estado sem a bola (para atualizar armados e turnos)
            engine->processNumber(0);

            // Gera status sincronizado após as reaberturas
            QJsonObject sync = getGameStatusJson(session.sorteioId);
            sync["action"] = "number_cancelled";
            sync["number"] = num;
            sync["reabertos"] = reabertos;
            broadcastToGame(session.sorteioId, sync);

            // CRITICAL FIX: Força uma sincronização completa para garantir que o painel atualize os status dos prêmios
            QJsonObject fullSync = getGameStatusJson(session.sorteioId);
            fullSync["action"] = "sync_status";
            broadcastToGame(session.sorteioId, fullSync);

            // REGRA DE SEGURANÇA: Reativa a chave se o sorteio não estiver mais concluído
            bool isFinished = sync["isFinished"].toBool();
            if (!isFinished && session.isOperator && session.chaveId > 0) {
                m_db->reativarChave(session.chaveId);
                qInfo() << "[SECURITY] Sorteio" << session.sorteioId << "REABERTO via Undo. Chave" << session.chaveId << "reativada.";
            }
        }
    }
    else if (action == "start_game" && session.isOperator) {
        // 1. Verificação de Segurança via Banco de Dados (Ultimate Source of Truth)
        QJsonObject chaveInfo = m_db->validarChaveAcesso(session.accessKey);
        if (chaveInfo.isEmpty() && session.chaveId > 0) {
             // Se a chave não for encontrada como 'ativa', provavelmente já foi bloqueada
             QJsonObject error;
             error["action"] = "draw_number_error";
             error["message"] = "Esta chave de acesso já foi utilizada para concluir um sorteio e não permite mais reinícios.";
             sendJson(client, error);
             qWarning() << "[SECURITY] Bloqueio de Reset: Chave já utilizada no DB. ChaveID:" << session.chaveId;
             return;
        }

        // 2. Bloqueio de reset para sorteios já concluídos (via estado do motor)
        QJsonObject currentStatus = getGameStatusJson(session.sorteioId);
        bool finished = currentStatus["isFinished"].toBool();
        
        qInfo() << "[DEBUG] Solicitação de RESET para SorteioID:" << session.sorteioId 
                << "Status isFinished:" << finished;

        if (finished) {
            QJsonObject error;
            error["action"] = "draw_number_error";
            error["message"] = "Este sorteio já foi concluído e não pode ser reiniciado. A chave de acesso foi inativada.";
            sendJson(client, error);

            // Garantia extra: inativa no banco se ainda não estiver
            if (session.chaveId > 0) m_db->bloquearChave(session.chaveId);
            return;
        }

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
    else if (action == "get_server_debug") {
        QJsonObject resp = engine->getDebugReport();
        resp["action"] = "server_debug_report";
        sendJson(client, resp);
        qInfo() << "BingoServer: Relatório de depuração solicitado pelo cliente. Enviado.";
    }
    else if (action == "add_rodada" && session.isOperator) {
        qInfo() << "[DEBUG] add_rodada: Recebido:" << json;

        QString nome = json["nome"].toString().trimmed();
        int baseId = json["base_id"].toInt();
        QJsonObject config = json["configuracoes"].toObject();
        int ordem = json["ordem"].toInt();
        QJsonArray premios = json["premios"].toArray();

        // VALIDAÇÃO: Evitar nomes duplicados
        QJsonArray existingRodadas = m_db->getRodadas(session.sorteioId);
        bool duplicado = false;
        for (int i = 0; i < existingRodadas.size(); ++i) {
            QString existingNome = existingRodadas[i].toObject()["nome"].toString().trimmed();
            if (existingNome.compare(nome, Qt::CaseInsensitive) == 0) {
                duplicado = true;
                break;
            }
        }

        if (duplicado) {
            qWarning() << "[DEBUG] add_rodada: Nome duplicado detectado:" << nome;
            QJsonObject error;
            error["action"] = "add_rodada_error";
            error["message"] = QString("Já existe uma rodada com o nome '%1'.").arg(nome);
            sendJson(client, error);
            return;
        }
        
        // 1. Salva a Rodada (Grupo Pai)
        int rodadaId = m_db->addRodada(session.sorteioId, nome, baseId, config, ordem);
        if (rodadaId > 0) {
            qInfo() << "[DEBUG] add_rodada: Rodada salva com ID:" << rodadaId << ". Salvando prêmios...";
            // 2. Salva cada Prêmio (Regras Filhas)
            for (int i = 0; i < premios.size(); ++i) {
                QJsonObject p = premios[i].toObject();
                m_db->addPremio(rodadaId, p["tipo"].toString(), p["descricao"].toString(), p["padrao"].toArray(), i);
            }
            qInfo() << "[DEBUG] add_rodada: Rodada e prêmios concluídos. Recarregando engine.";

            // Força recarga do motor para incluir a nova estrutura
            if (m_gameInstances.contains(session.sorteioId)) {
                delete m_gameInstances[session.sorteioId].engine;
                m_gameInstances.remove(session.sorteioId);
            }
            getEngine(session.sorteioId);

            QJsonObject resp;
            resp["action"] = "rodada_added";
            resp["status"] = "ok";
            sendJson(client, resp);
            
            QJsonObject sync = getGameStatusJson(session.sorteioId);
            sync["action"] = "sync_status";
            broadcastToGame(session.sorteioId, sync);
        } else {
            qCritical() << "[DEBUG] add_rodada: Falha ao salvar no DB!";
            QJsonObject error;
            error["action"] = "add_rodada_error";
            error["message"] = "O banco de dados rejeitou o salvamento da rodada.";
            sendJson(client, error);
        }
    } else if (action == "add_rodada") {
        qWarning() << "[SECURITY] Tentativa de add_rodada sem permissão de operador.";
        QJsonObject error;
        error["action"] = "add_rodada_error";
        error["message"] = "Você não tem permissão de operador para realizar esta ação.";
        sendJson(client, error);
    }
    else if (action == "delete_rodada" && session.isOperator) {
        int rodadaId = json["id"].toInt();
        if (m_db->removerRodada(rodadaId)) {
            // Recarrega prêmios no motor para sincronizar
            engine->clearPrizes();
            QJsonArray dbRodadas = m_db->getRodadas(session.sorteioId);
            for(int i=0; i<dbRodadas.size(); ++i) {
                QJsonObject rObj = dbRodadas[i].toObject();
                QJsonArray premios = rObj["premios"].toArray();
                for(int j=0; j<premios.size(); ++j) {
                    QJsonObject po = premios[j].toObject();
                    Prize p;
                    p.id = po["id"].toInt();
                    p.nome = QString("%1 (%2)").arg(rObj["nome"].toString(), po["tipo"].toString().toUpper());
                    p.tipo = po["tipo"].toString();
                    p.active = true;
                    p.realizada = po["realizada"].toBool();
                    QJsonArray pad = po["padrao"].toArray();
                    for(const QJsonValue &v : pad) p.padraoIndices.insert(v.toInt());
                    engine->addPrize(p);
                }
            }

            QJsonObject resp;
            resp["action"] = "rodada_deleted";
            resp["id"] = rodadaId;
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
        
        QSet<int> availableIds;
        // Busca IDs em todos os motores carregados (bases)
        // Como o ID da cartela é 1-N, podemos apenas olhar o tamanho da maior base carregada
        // ou assumir um limite. Aqui vamos buscar nas bases do motor.
        auto prizes = engine->getPrizes();
        if (!prizes.isEmpty()) {
            prizes.first().baseId;
            // Isso ainda é uma simplificação, mas resolve o erro de compilação
            // e funciona se os IDs forem sequenciais.
            for (int i = 1; i <= 10000; ++i) { // Limite arbitrário para teste
                if (!engine->isTicketRegistered(i)) {
                    availableIds.insert(i);
                }
            }
        }

        QList<int> availableList = availableIds.toList();
        std::shuffle(availableList.begin(), availableList.end(), *QRandomGenerator::global());

        int limit = qMin(count, availableList.size());
        for(int i = 0; i < limit; ++i) {
            int tid = availableList[i];
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

QJsonObject BingoServer::getTicketDetailsJson(int sorteioId, int ticketId, int baseId)
{
    QJsonObject obj;
    BingoGameEngine *engine = getEngine(sorteioId);
    if (!engine) return obj;

    obj["barcode"] = engine->getFormattedBarcode(ticketId);
    obj["ticketId"] = ticketId;
    
    QJsonArray nums;
    // Se não forneceu baseId (-1), tenta encontrar uma base válida para este ticket
    int actualBaseId = baseId;
    if (actualBaseId == -1) {
        // Mock simples: pega a primeira base carregada no motor
        // No futuro o frontend deve pedir uma específica
        auto prizes = engine->getPrizes();
        if (!prizes.isEmpty()) actualBaseId = prizes.first().baseId;
    }

    for(int n : engine->getTicketNumbers(actualBaseId, ticketId)) {
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
    
    // Winners (Globais - pegamos a base do primeiro prêmio ou base padrão se disponível)
    QJsonArray winnersArray;
    int firstBaseId = -1;
    if (!engine->getPrizes().isEmpty()) firstBaseId = engine->getPrizes().first().baseId;

    for(int w : engine->getWinners()) 
        winnersArray.append(getTicketDetailsJson(sorteioId, w, firstBaseId));
    sync["winners"] = winnersArray;

    // Near Wins (Boas)
    QJsonObject nearWins;
    auto nwMap = engine->getNearWinTickets();
    for(auto it = nwMap.begin(); it != nwMap.end(); ++it) {
        QJsonArray arr;
        int count = 0;
        for(int id : it.value()) {
            arr.append(getTicketDetailsJson(sorteioId, id, firstBaseId));
            if (++count >= 10) break;
        }
        nearWins[QString::number(it.key())] = arr;
    }
    sync["near_wins"] = nearWins;
    
    // Rodadas e Prêmios (Sempre enviamos a estrutura aninhada do DB para a UI)
    QJsonArray rodadas = m_db->getRodadas(sorteioId);
    sync["rodadas"] = rodadas;
    
    // Status em tempo real de cada sub-prêmio (do motor)
    QJsonArray engineStatus;
    for(const auto &p : engine->getPrizes()) {
        QJsonObject po;
        po["id"] = p.id;
        po["nome"] = p.nome;
        po["tipo"] = p.tipo;
        po["realizada"] = p.realizada;
        po["active"] = p.active;
        
        QJsonArray pWinners;
        for(int id : p.winners) {
            QJsonObject winObj = getTicketDetailsJson(sorteioId, id, p.baseId);
            if (p.winnerPatterns.contains(id)) {
                QJsonArray dynPadrao;
                for(int idx : p.winnerPatterns[id]) dynPadrao.append(idx);
                winObj["padrao"] = dynPadrao;
            }
            pWinners.append(winObj);
        }
        po["winners"] = pWinners;

        QJsonArray pNearWinners;
        for(int id : p.near_winners) pNearWinners.append(getTicketDetailsJson(sorteioId, id, p.baseId));
        po["near_winners"] = pNearWinners;

        QJsonArray padraoArr;
        for(int idx : p.padraoIndices) padraoArr.append(idx);
        po["padrao"] = padraoArr;

        engineStatus.append(po);
    }
    sync["engine_premios_status"] = engineStatus;

    QJsonObject sorteio = m_db->getSorteio(sorteioId);
    // Configurações globais agora podem vir de configurações por prêmio ou de um modelo
    // mantemos defaults seguros.
    sync["historyLimit"] = 10;
    sync["numChances"] = 1;
    sync["maxBalls"] = 75; 

    // Injeta tipo_grade no sync para o badge do topo
    if (!rodadas.isEmpty()) {
        sync["tipo_grade"] = rodadas[0].toObject()["tipo_grade"].toString();
    } else {
        sync["tipo_grade"] = "75x15";
    }

    sync["data_sorteio"] = sorteio["data_sorteio"].toString();
    sync["hora_inicio"] = sorteio["hora_inicio"].toString();
    sync["hora_fim"] = sorteio["hora_fim"].toString();

    // Verificação de conclusão
    bool allRealized = true;
    auto prizesList = engine->getPrizes();
    if (prizesList.isEmpty()) {
        allRealized = false;
        qDebug() << "[DEBUG] getGameStatusJson: SorteioID" << sorteioId << "sem prêmios - isFinished=false";
    } else {
        QString debugPrizes;
        for(const auto &p : prizesList) {
            debugPrizes += QString("[%1: A=%2, R=%3] ").arg(p.nome).arg(p.active).arg(p.realizada);
            if (p.active && !p.realizada) {
                allRealized = false;
                // Não break aqui para logar todos os estados se necessário, mas para performance:
                // break; 
            }
        }
        if (!allRealized) {
             qDebug() << "[DEBUG] getGameStatusJson: SorteioID" << sorteioId << "em aberto. Estados:" << debugPrizes;
        } else {
             qInfo() << "[DEBUG] getGameStatusJson: SorteioID" << sorteioId << "CONCLUÍDO. Todos prêmios realizados.";
        }
    }
    sync["isFinished"] = allRealized;

    return sync;
}
