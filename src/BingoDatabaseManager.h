#ifndef BINGODATABASEMANAGER_H
#define BINGODATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>

class BingoDatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit BingoDatabaseManager(QObject *parent = nullptr);
    bool connectToDatabase(const QString &host, const QString &dbName, const QString &user, const QString &pass);
    bool executarScriptSQL(const QString &caminho);

    // Chaves de Acesso
    QJsonObject validarChaveAcesso(const QString &chave);
    bool bloquearChave(int chaveId);

    // Sorteios (Global/Admin)
    QJsonArray listarTodosSorteios();
    QJsonArray listarTodasChavesAcesso();
    QJsonArray listarModelos();
    QJsonArray listarBases();
    int criarSorteioComChave(int modeloId, int baseId, const QString &chave);

    // Sorteios (Individuais)
    QJsonObject getSorteio(int sorteioId);
    bool atualizarStatusSorteio(int sorteioId, const QString &status);
    bool atualizarConfigSorteio(int sorteioId, int modeloId, int baseId, const QJsonObject &preferencias);
    bool salvarSorteioComoModelo(const QString &nome, const QJsonObject &config);
    bool salvarBolaSorteada(int sorteioId, int numero);
    bool removerUltimaBola(int sorteioId, int numero);
    bool limparSorteio(int sorteioId);
    QList<int> getBolasSorteadas(int sorteioId);

    // Cartelas
    bool registrarVenda(int sorteioId, int numeroCartela, const QString &telefone = "", const QString &origem = "Manual");
    QList<int> getCartelasValidadas(int sorteioId);
    QList<int> getCartelasPorTelefone(int sorteioId, const QString &telefone);

    // Premiações
    QJsonArray getPremiacoes(int sorteioId);
    bool addPremiacao(int sorteioId, const QString &nome, const QString &tipo, const QJsonArray &padrao = QJsonArray(), int ordem = 0);
    bool removerPremiacao(int premioId);
    bool atualizarStatusPremiacao(int premioId, bool realizada);

private:
    QSqlDatabase m_db;
};

#endif // BINGODATABASEMANAGER_H
