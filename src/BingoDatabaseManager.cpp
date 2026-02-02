#include "BingoDatabaseManager.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>

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

    if (!query.exec() || !query.next()) {
        return QJsonObject();
    }

    QJsonObject obj;
    obj["id"] = query.value("id").toInt();
    obj["sorteio_id"] = query.value("sorteio_id").toInt();
    obj["status"] = query.value("status").toString();
    obj["sorteio_status"] = query.value("sorteio_status").toString();
    return obj;
}

QJsonObject BingoDatabaseManager::getSorteio(int sorteioId)
{
    QSqlQuery query;
    query.prepare("SELECT s.*, b.tipo_grade, b.caminho_dados, m.nome as modelo_nome "
                  "FROM SORTEIOS s "
                  "JOIN BASES_DADOS b ON b.id = s.base_id "
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

    QString tipoGrade = query.value("tipo_grade").toString();
    QString caminhoDados = query.value("caminho_dados").toString();
    qInfo() << "getSorteio:" << sorteioId << "tipo_grade=" << tipoGrade << "caminho=" << caminhoDados;

    QJsonObject obj;
    obj["id"] = query.value("id").toInt();
    obj["status"] = query.value("status").toString();
    obj["modelo_id"] = query.value("modelo_id").toInt();
    obj["base_id"] = query.value("base_id").toInt();
    obj["tipo_grade"] = tipoGrade;
    obj["caminho_dados"] = caminhoDados;
    obj["modelo_nome"] = query.value("modelo_nome").toString();
    
    QString prefsStr = query.value("preferencias").toString();
    if (!prefsStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(prefsStr.toUtf8());
        obj["preferencias"] = doc.object();
    }
    
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

QJsonArray BingoDatabaseManager::getPremiacoes(int sorteioId)
{
    QJsonArray array;
    QSqlQuery query;
    query.prepare("SELECT * FROM PREMIACOES WHERE sorteio_id = :sid ORDER BY ordem_exibicao ASC, id ASC");
    query.bindValue(":sid", sorteioId);
    if (query.exec()) {
        while (query.next()) {
            QJsonObject obj;
            obj["id"] = query.value("id").toInt();
            obj["nome"] = query.value("nome_premio").toString();
            obj["tipo"] = query.value("tipo").toString();
            
            QString padraoStr = query.value("padrao_grade").toString();
            if (!padraoStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(padraoStr.toUtf8());
                obj["padrao"] = doc.array();
            }
            array.append(obj);
        }
    }
    return array;
}

bool BingoDatabaseManager::addPremiacao(int sorteioId, const QString &nome, const QString &tipo, const QJsonArray &padrao, int ordem)
{
    QSqlQuery query;
    query.prepare("INSERT INTO PREMIACOES (sorteio_id, nome_premio, tipo, padrao_grade, ordem_exibicao) "
                  "VALUES (:sid, :nome, :tipo, :padrao, :ordem)");
    query.bindValue(":sid", sorteioId);
    query.bindValue(":nome", nome);
    query.bindValue(":tipo", tipo);
    
    QJsonDocument doc(padrao);
    query.bindValue(":padrao", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    query.bindValue(":ordem", ordem);
    
    return query.exec();
}

bool BingoDatabaseManager::removerPremiacao(int premioId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM PREMIACOES WHERE id = :id");
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
    QSqlQuery query("SELECT s.*, m.nome as modelo_nome, b.nome as base_nome "
                    "FROM SORTEIOS s "
                    "LEFT JOIN MODELOS_SORTEIO m ON s.modelo_id = m.id "
                    "LEFT JOIN BASES_DADOS b ON s.base_id = b.id "
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
        obj["base"] = query.value("base_nome").toString();
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

int BingoDatabaseManager::criarSorteioComChave(int modeloId, int baseId, const QString &chave)
{
    if (!m_db.transaction()) return -1;

    QSqlQuery qSorteio;
    qSorteio.prepare("INSERT INTO SORTEIOS (modelo_id, base_id, status) VALUES (:mid, :bid, 'configurando') RETURNING id");
    qSorteio.bindValue(":mid", modeloId);
    qSorteio.bindValue(":bid", baseId);
    
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
bool BingoDatabaseManager::atualizarConfigSorteio(int sorteioId, int modeloId, int baseId, const QJsonObject &preferencias)
{
    QSqlQuery query;
    query.prepare("UPDATE SORTEIOS SET modelo_id = :mid, base_id = :bid, preferencias = :prefs WHERE id = :sid");
    query.bindValue(":mid", modeloId);
    query.bindValue(":bid", baseId);
    
    QJsonDocument doc(preferencias);
    query.bindValue(":prefs", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
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
