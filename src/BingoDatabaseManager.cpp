#include "BingoDatabaseManager.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QTextStream>

BingoDatabaseManager::BingoDatabaseManager(QObject *parent) : QObject(parent)
{
}

bool BingoDatabaseManager::connectToDatabase(const QString &host, const QString &dbName, const QString &user, const QString &pass)
{
    qInfo() << "Tentando conectar ao banco:" << host << "BD:" << dbName << "User:" << user << "Senha (size):" << pass.length();
    m_db = QSqlDatabase::addDatabase("QPSQL");
    m_db.setHostName(host);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(pass);

    if (!m_db.open()) {
        qCritical() << "Erro ao conectar ao PostgreSQL:" << m_db.lastError().text();
        return false;
    }
    qInfo() << "Conectado ao PostgreSQL com sucesso!";
    return true;
}

QJsonObject BingoDatabaseManager::validarChaveAcesso(const QString &chave)
{
    QSqlQuery query;
    query.prepare("SELECT c.id, c.sorteio_id, c.status, s.status as sorteio_status "
                  "FROM CHAVES_ACESSO c "
                  "JOIN SORTEIOS s ON s.id = c.sorteio_id "
                  "WHERE c.codigo_chave = :chave AND c.status = 'ativa'");
    query.bindValue(":chave", chave);

    qInfo() << "[DEBUG] validarChaveAcesso: Verificando chave:" << chave;

    if (!query.exec()) {
        qCritical() << "[DEBUG] validarChaveAcesso: Erro na query:" << query.lastError().text();
        return QJsonObject();
    }
    
    if (!query.next()) {
        qInfo() << "[DEBUG] validarChaveAcesso: Nenhuma linha encontrada para chave:" << chave;
        return QJsonObject();
    }

    QJsonObject obj;
    obj["id"] = query.value("id").toInt();
    obj["sorteio_id"] = query.value("sorteio_id").toInt();
    obj["status"] = query.value("status").toString();
    obj["sorteio_status"] = query.value("sorteio_status").toString();
    
    qInfo() << "[DEBUG] validarChaveAcesso: Sucesso! ID:" << obj["id"].toInt() << "Sorteio:" << obj["sorteio_id"].toInt();
    
    return obj;
}

bool BingoDatabaseManager::bloquearChave(int chaveId)
{
    QSqlQuery query;
    query.prepare("UPDATE CHAVES_ACESSO SET status = 'utilizada' WHERE id = :id");
    query.bindValue(":id", chaveId);
    if (!query.exec()) {
        qCritical() << "Erro ao inativar chave:" << query.lastError().text();
        return false;
    }
    return true;
}

bool BingoDatabaseManager::reativarChave(int chaveId)
{
    QSqlQuery query;
    query.prepare("UPDATE CHAVES_ACESSO SET status = 'ativa' WHERE id = :id");
    query.bindValue(":id", chaveId);
    if (!query.exec()) {
        qCritical() << "Erro ao reativar chave:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonObject BingoDatabaseManager::getSorteio(int sorteioId)
{
    QSqlQuery query;
    query.prepare("SELECT s.*, m.nome as modelo_nome "
                  "FROM SORTEIOS s "
                  "JOIN MODELOS_SORTEIO m ON m.id = s.modelo_id "
                  "WHERE s.id = :id");
    query.bindValue(":id", sorteioId);

    if (!query.exec()) {
        qWarning() << "getSorteio: Query falhou:" << query.lastError().text();
        return QJsonObject();
    }
    if (!query.next()) {
        qWarning() << "getSorteio: Nenhum resultado para sorteio:" << sorteioId;
        return QJsonObject();
    }

    QJsonObject obj;
    obj["id"] = query.value("id").toInt();
    obj["status"] = query.value("status").toString();
    obj["modelo_id"] = query.value("modelo_id").toInt();
    obj["modelo_nome"] = query.value("modelo_nome").toString();
    obj["data_sorteio"] = query.value("data_sorteio").toDate().toString(Qt::ISODate);
    obj["hora_inicio"] = query.value("hora_sorteio_inicio").toTime().toString(Qt::ISODate);
    obj["hora_fim"] = query.value("hora_sorteio_fim").toTime().toString(Qt::ISODate);
    
    return obj;
}

bool BingoDatabaseManager::salvarBolaSorteada(int sorteioId, int numero)
{
    QSqlQuery query;
    query.prepare("INSERT INTO BOLAS_SORTEADAS (sorteio_id, numero) VALUES (:sid, :num)");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":num", numero);
    return query.exec();
}

QList<int> BingoDatabaseManager::getBolasSorteadas(int sorteioId)
{
    QList<int> bolas;
    QSqlQuery query;
    query.prepare("SELECT numero FROM BOLAS_SORTEADAS WHERE sorteio_id = :sid ORDER BY momento ASC");
    query.bindValue(":sid", sorteioId);
    if (query.exec()) {
        while (query.next()) bolas.append(query.value(0).toInt());
    }
    return bolas;
}

bool BingoDatabaseManager::registrarVenda(int sorteioId, int numeroCartela, const QString &telefone, const QString &origem)
{
    QSqlQuery query;
    query.prepare("INSERT INTO CARTELAS_VALIDADAS (sorteio_id, numero_cartela, telefone_participante, origem) "
                  "VALUES (:sid, :num, :tel, :ori)");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":num", numeroCartela);
    query.bindValue(":tel", telefone.isEmpty() ? QVariant(QVariant::String) : telefone);
    query.bindValue(":ori", origem);
    if (!query.exec()) {
        qCritical() << "Erro ao registrar venda:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<int> BingoDatabaseManager::getCartelasValidadas(int sorteioId)
{
    QList<int> cartelas;
    QSqlQuery query;
    query.prepare("SELECT numero_cartela FROM CARTELAS_VALIDADAS WHERE sorteio_id = :sid");
    query.bindValue(":sid", sorteioId);
    if (query.exec()) {
        while (query.next()) cartelas.append(query.value(0).toInt());
    }
    return cartelas;
}

QJsonArray BingoDatabaseManager::getRodadas(int sorteioId)
{
    QJsonArray array;
    QSqlQuery query;
    // Seleciona as Rodadas (Pai)
    query.prepare("SELECT r.*, b.tipo_grade, b.caminho_dados "
                  "FROM RODADAS r "
                  "LEFT JOIN BASES_DADOS b ON b.id = r.base_id "
                  "WHERE r.sorteio_id = :sid "
                  "ORDER BY r.ordem_exibicao ASC, r.id ASC");
    query.bindValue(":sid", sorteioId);
    
    if (query.exec()) {
        while (query.next()) {
            QJsonObject rodadaObj;
            int rodadaId = query.value("id").toInt();
            rodadaObj["id"] = rodadaId;
            rodadaObj["nome"] = query.value("nome_rodada").toString();
            rodadaObj["base_id"] = query.value("base_id").toInt();
            rodadaObj["tipo_grade"] = query.value("tipo_grade").toString();
            rodadaObj["caminho_dados"] = query.value("caminho_dados").toString();
            
            QString configStr = query.value("configuracoes").toString();
            if (!configStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(configStr.toUtf8());
                rodadaObj["configuracoes"] = doc.object();
            }

            // Agora busca os Prêmios vinculados (Filhos)
            QJsonArray premiosArray;
            QSqlQuery subQuery;
            subQuery.prepare("SELECT * FROM PREMIOS WHERE rodada_id = :rid ORDER BY ordem_exibicao ASC, id ASC");
            subQuery.bindValue(":rid", rodadaId);
            if (subQuery.exec()) {
                while (subQuery.next()) {
                    QJsonObject premioObj;
                    premioObj["id"] = subQuery.value("id").toInt();
                    premioObj["tipo"] = subQuery.value("tipo").toString();
                    premioObj["descricao"] = subQuery.value("descricao").toString();
                    premioObj["realizada"] = subQuery.value("realizada").toBool();
                    
                    QString padraoStr = subQuery.value("padrao_grade").toString();
                    if (!padraoStr.isEmpty()) {
                        QJsonDocument doc = QJsonDocument::fromJson(padraoStr.toUtf8());
                        premioObj["padrao"] = doc.array();
                    }
                    premiosArray.append(premioObj);
                }
            }
            rodadaObj["premios"] = premiosArray;
            array.append(rodadaObj);
        }
    }
    return array;
}

int BingoDatabaseManager::addRodada(int sorteioId, const QString &nome, int baseId, const QJsonObject &configuracoes, int ordem)
{
    QSqlQuery query;
    query.prepare("INSERT INTO RODADAS (sorteio_id, nome_rodada, base_id, configuracoes, ordem_exibicao) "
                  "VALUES (:sid, :nome, :bid, :config, :ordem) RETURNING id");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":nome", nome);
    query.bindValue(":bid", baseId);

    QJsonDocument docConfig(configuracoes);
    query.bindValue(":config", QString::fromUtf8(docConfig.toJson(QJsonDocument::Compact)));
    query.bindValue(":ordem", ordem);

    qInfo() << "[DEBUG] addRodada: Tentando inserir rodada:" << nome << "Base:" << baseId;

    if (!query.exec()) {
        qCritical() << "[DEBUG] addRodada: Erro SQL:" << query.lastError().text();
        return -1;
    }
    
    if (query.next()) {
        int id = query.value(0).toInt();
        qInfo() << "[DEBUG] addRodada: Sucesso! ID:" << id;
        return id;
    }
    
    qCritical() << "[DEBUG] addRodada: Query executou mas nao retornou ID.";
    return -1;
}

bool BingoDatabaseManager::addPremio(int rodadaId, const QString &tipo, const QString &descricao, const QJsonArray &padrao, int ordem)
{
    QSqlQuery query;
    query.prepare("INSERT INTO PREMIOS (rodada_id, tipo, descricao, padrao_grade, ordem_exibicao) "
                  "VALUES (:rid, :tipo, :desc, :padrao, :ordem)");
    query.bindValue(":rid", rodadaId);
    query.bindValue(":tipo", tipo);
    query.bindValue(":desc", descricao);
    
    QJsonDocument docPadrao(padrao);
    query.bindValue(":padrao", QString::fromUtf8(docPadrao.toJson(QJsonDocument::Compact)));
    query.bindValue(":ordem", ordem);
    
    return query.exec();
}

bool BingoDatabaseManager::removerRodada(int rodadaId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM RODADAS WHERE id = :id");
    query.bindValue(":id", rodadaId);
    return query.exec();
}

bool BingoDatabaseManager::removerPremio(int premioId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM PREMIOS WHERE id = :id");
    query.bindValue(":id", premioId);
    return query.exec();
}

bool BingoDatabaseManager::atualizarStatusPremio(int premioId, bool realizada)
{
    QSqlQuery query;
    query.prepare("UPDATE PREMIOS SET realizada = :status WHERE id = :id");
    query.bindValue(":status", realizada);
    query.bindValue(":id", premioId);
    return query.exec();
}

QList<int> BingoDatabaseManager::getCartelasPorTelefone(int sorteioId, const QString &telefone)
{
    QList<int> cartelas;
    QSqlQuery query;
    query.prepare("SELECT numero_cartela FROM CARTELAS_VALIDADAS WHERE sorteio_id = :sid AND telefone_participante = :tel");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":tel", telefone);
    if (query.exec()) {
        while (query.next()) cartelas.append(query.value(0).toInt());
    }
    return cartelas;
}

QJsonArray BingoDatabaseManager::listarTodosSorteios()
{
    QJsonArray array;
    QSqlQuery query("SELECT s.*, m.nome as modelo_nome "
                    "FROM SORTEIOS s "
                    "LEFT JOIN MODELOS_SORTEIO m ON s.modelo_id = m.id "
                    "ORDER BY s.id DESC");
    
    if (!query.isActive() && !query.exec()) {
        qCritical() << "Erro ao listar sorteios:" << query.lastError().text();
        return array;
    }
    
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value("id").toInt();
        obj["status"] = query.value("status").toString();
        obj["modelo"] = query.value("modelo_nome").toString();
        obj["data"] = query.value("data_sorteio").toDate().toString("dd/MM/yyyy");
        obj["criado_em"] = query.value("criado_em").toDateTime().toString("dd/MM/yyyy HH:mm");
        array.append(obj);
    }
    return array;
}

QJsonArray BingoDatabaseManager::listarTodasChavesAcesso()
{
    QJsonArray array;
    QSqlQuery query("SELECT * FROM CHAVES_ACESSO ORDER BY id DESC");
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value("id").toInt();
        obj["chave"] = query.value("codigo_chave").toString();
        obj["status"] = query.value("status").toString();
        obj["sorteio_id"] = query.value("sorteio_id").toInt();
        array.append(obj);
    }
    return array;
}

QJsonArray BingoDatabaseManager::listarModelos()
{
    QJsonArray array;
    QSqlQuery query("SELECT id, nome, config_padrao FROM MODELOS_SORTEIO ORDER BY id");
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value("id").toInt();
        obj["nome"] = query.value("nome").toString();
        
        QString configStr = query.value("config_padrao").toString();
        if (!configStr.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(configStr.toUtf8());
            obj["config"] = doc.object();
        }
        
        array.append(obj);
    }
    return array;
}

QJsonArray BingoDatabaseManager::listarBases()
{
    QJsonArray array;
    QSqlQuery query("SELECT id, nome, tipo_grade FROM BASES_DADOS ORDER BY id");
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value("id").toInt();
        obj["nome"] = query.value("nome").toString();
        obj["tipo"] = query.value("tipo_grade").toString();
        array.append(obj);
    }
    return array;
}

int BingoDatabaseManager::criarSorteioComChave(int modeloId, const QString &chave, const QDate &data, const QTime &horaInicio, const QTime &horaFim)
{
    if (!m_db.transaction()) return -1;

    QSqlQuery qSorteio;
    qSorteio.prepare("INSERT INTO SORTEIOS (modelo_id, status, data_sorteio, hora_sorteio_inicio, hora_sorteio_fim) "
                  "VALUES (:mid, 'configurando', :data, :hini, :hfim) RETURNING id");
    qSorteio.bindValue(":mid", modeloId);
    qSorteio.bindValue(":data", data.isValid() ? data : QVariant(QVariant::Date));
    qSorteio.bindValue(":hini", horaInicio.isValid() ? horaInicio : QVariant(QVariant::Time));
    qSorteio.bindValue(":hfim", horaFim.isValid() ? horaFim : QVariant(QVariant::Time));
    
    if (!qSorteio.exec() || !qSorteio.next()) {
        qCritical() << "Erro ao inserir sorteio:" << qSorteio.lastError().text();
        m_db.rollback();
        return -1;
    }

    int sorteioId = qSorteio.value(0).toInt();

    QSqlQuery qChave;
    qChave.prepare("INSERT INTO CHAVES_ACESSO (codigo_chave, sorteio_id, status) VALUES (:chave, :sid, 'ativa')");
    qChave.bindValue(":chave", chave);
    qChave.bindValue(":sid", sorteioId);

    if (!qChave.exec()) {
        qCritical() << "Erro ao inserir chave:" << qChave.lastError().text();
        m_db.rollback();
        return -1;
    }

    if (!m_db.commit()) return -1;

    return sorteioId;
}
bool BingoDatabaseManager::atualizarConfigSorteio(int sorteioId, int modeloId, const QJsonObject &/*configuracoes*/)
{
    // Nota: O campo 'preferencias' foi removido em favor de configurações por prêmio.
    // Mantemos este método se houver configurações globais no futuro, ou o removemos.
    QSqlQuery query;
    query.prepare("UPDATE SORTEIOS SET modelo_id = :mid WHERE id = :sid");
    query.bindValue(":mid", modeloId);
    query.bindValue(":sid", sorteioId);
    
    return query.exec();
}

bool BingoDatabaseManager::atualizarAgendamentoSorteio(int sorteioId, const QDate &data, const QTime &horaInicio, const QTime &horaFim)
{
    QSqlQuery query;
    query.prepare("UPDATE SORTEIOS SET data_sorteio = :data, hora_sorteio_inicio = :hini, hora_sorteio_fim = :hfim WHERE id = :sid");
    query.bindValue(":data", data);
    query.bindValue(":hini", horaInicio);
    query.bindValue(":hfim", horaFim);
    query.bindValue(":sid", sorteioId);
    return query.exec();
}

bool BingoDatabaseManager::salvarSorteioComoModelo(const QString &nome, const QJsonObject &config)
{
    QSqlQuery query;
    query.prepare("INSERT INTO MODELOS_SORTEIO (nome, config_padrao) VALUES (:nome, :config)");
    query.bindValue(":nome", nome);
    
    QJsonDocument doc(config);
    query.bindValue(":config", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    
    return query.exec();
}

bool BingoDatabaseManager::removerUltimaBola(int sorteioId, int numero)
{
    QSqlQuery query;
    query.prepare("DELETE FROM BOLAS_SORTEADAS WHERE sorteio_id = :sid AND numero = :num");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":num", numero);
    return query.exec();
}

bool BingoDatabaseManager::limparSorteio(int sorteioId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM BOLAS_SORTEADAS WHERE sorteio_id = :sid");
    query.bindValue(":sid", sorteioId);
    if (!query.exec()) {
        qCritical() << "Erro ao limpar sorteio (bolas):" << query.lastError().text();
        return false;
    }
    
    qInfo() << "Sorteio" << sorteioId << "limpo com sucesso no banco de dados.";
    return true;
}


bool BingoDatabaseManager::executarScriptSQL(const QString &caminho)
{
    QFile arquivo(caminho);
    if (!arquivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "executarScriptSQL: Nao foi possivel abrir o arquivo:" << caminho;
        return false;
    }

    QTextStream in(&arquivo);
    QString script = in.readAll();
    arquivo.close();

    // Divide o script por ponto e vírgula para executar cada comando individualmente
    // NOTA: Esta lógica é simples e assume que não há ";" dentro de strings ou comentários.
    // Para scripts mais complexos, o ideal seria um parser de SQL real.
    QStringList comandos = script.split(';', QString::SkipEmptyParts);
    
    if (!m_db.transaction()) {
        qCritical() << "executarScriptSQL: Falha ao iniciar transacao.";
        return false;
    }

    for (const QString &cmd : comandos) {
        QString sql = cmd.trimmed();
        if (sql.isEmpty()) continue;

        QSqlQuery query;
        if (!query.exec(sql)) {
            qCritical() << "executarScriptSQL: Erro ao executar comando:" << query.lastError().text();
            qCritical() << "SQL:" << sql;
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        qCritical() << "executarScriptSQL: Falha ao commitar transacao.";
        return false;
    }

    qInfo() << "executarScriptSQL: Script" << caminho << "executado com sucesso!";
    return true;
}
