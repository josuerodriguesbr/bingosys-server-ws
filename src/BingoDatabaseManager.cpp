#include "BingoDatabaseManager.h"
#include <QDebug>
#include <QDateTime>

BingoDatabaseManager::BingoDatabaseManager(QObject *parent) : QObject(parent)
{
}

bool BingoDatabaseManager::connectToDatabase(const QString &host, const QString &dbName, const QString &user, const QString &pass)
{
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
    query.prepare("SELECT * FROM SORTEIOS WHERE id = :id");
    query.bindValue(":id", sorteioId);

    if (!query.exec() || !query.next()) return QJsonObject();

    QJsonObject obj;
    obj["status"] = query.value("status").toString();
    obj["modelo_id"] = query.value("modelo_id").toInt();
    obj["base_id"] = query.value("base_id").toInt();
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
    return query.exec();
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
    query.prepare("SELECT * FROM PREMIACOES WHERE sorteio_id = :sid ORDER BY ordem_exibicao ASC");
    query.bindValue(":sid", sorteioId);
    if (query.exec()) {
        while (query.next()) {
            QJsonObject obj;
            obj["nome"] = query.value("nome_premio").toString();
            obj["tipo"] = query.value("tipo").toString();
            // JSONB padrao_grade pode ser extraído como QJsonDocument
            array.append(obj);
        }
    }
    return array;
}
