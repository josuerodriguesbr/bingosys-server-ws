#ifndef BINGODATABASEMANAGER_H
#define BINGODATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QDate>
#include <QTime>

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
    bool reativarChave(int chaveId);

    // Sorteios (Global/Admin)
    QJsonArray listarTodosSorteios();
    QJsonArray listarTodasChavesAcesso();
    QJsonArray listarModelos();
    QJsonArray listarBases();
    int criarSorteioComChave(int modeloId, const QString &chave, const QDate &data = QDate(), const QTime &horaInicio = QTime(), const QTime &horaFim = QTime());

    // Sorteios (Individuais)
    QJsonObject getSorteio(int sorteioId);
    bool atualizarStatusSorteio(int sorteioId, const QString &status);
    bool atualizarConfigSorteio(int sorteioId, int modeloId, const QJsonObject &configuracoes = QJsonObject());
    bool atualizarAgendamentoSorteio(int sorteioId, const QDate &data, const QTime &horaInicio, const QTime &horaFim);
    bool salvarSorteioComoModelo(const QString &nome, const QJsonObject &config);
    bool salvarBolaSorteada(int sorteioId, int numero);
    bool removerUltimaBola(int sorteioId, int numero);
    bool limparSorteio(int sorteioId);
    QList<int> getBolasSorteadas(int sorteioId);

    // Cartelas
    bool registrarVenda(int sorteioId, int numeroCartela, const QString &telefone = "", const QString &origem = "Manual");
    QList<int> getCartelasValidadas(int sorteioId);
    QList<int> getCartelasPorTelefone(int sorteioId, const QString &telefone);

    // Rodadas e Prêmios
    QJsonArray getRodadas(int sorteioId);
    int addRodada(int sorteioId, const QString &nome, int baseId, const QJsonObject &configuracoes = QJsonObject(), int ordem = 0);
    bool addPremio(int rodadaId, const QString &tipo, const QString &descricao, const QJsonArray &padrao = QJsonArray(), int ordem = 0);
    bool removerRodada(int rodadaId);
    bool removerPremio(int premioId);
    bool atualizarStatusPremio(int premioId, bool realizada);

private:
    QSqlDatabase m_db;
};

#endif // BINGODATABASEMANAGER_H
