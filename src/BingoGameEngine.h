#ifndef BINGOGAMEENGINE_H
#define BINGOGAMEENGINE_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include "BingoTicketParser.h"

struct TicketState {
    int ticketId;
    int baseId; // Qual base este estado representa
    int matches; 
    QSet<int> missingNumbers; 
    int totalNumbers;
    QSet<int> wonPrizeIds; 
    QMap<QString, QList<QList<int>>> usedPatterns; 
};

struct Prize {
    int id;
    QString nome;
    QString tipo; 
    int baseId;      // Base de cartelas para este prêmio
    int gridIndex;   // Qual grade da base usar
    QJsonObject configuracoes; // Configurações específicas (ex: acumula, etc)
    QSet<int> padraoIndices; 
    QList<int> winners;
    QList<int> near_winners;
    QMap<int, QList<int>> winnerPatterns; 
    bool active;
    bool realizada;
};

class BingoGameEngine : public QObject
{
    Q_OBJECT
public:
    explicit BingoGameEngine(QObject *parent = nullptr);

    // Carrega as cartelas de uma base para o jogo
    void loadBase(int baseId, const QVector<BingoTicket> &tickets);

    // O modo agora é definido por prêmio, mas mantemos o global para compatibilidade se necessário
    void setGameMode(int gridIndex); 
    int getGameMode() const { return m_currentGridIndex; }

    void setMaxBalls(int max) { m_maxBalls = max; }
    int getMaxBalls() const { return m_maxBalls; }

    void setNumChances(int chances) { m_numChances = chances; }
    int getNumChances() const { return m_numChances; }

    // Inicia um novo sorteio (limpa estado)
    // Se houver cartelas registradas, usa apenas elas.
    void startNewGame();

    // Gestão de Vendas (Cartelas Registradas)
    void registerTicket(int ticketId);
    void unregisterTicket(int ticketId);
    void clearRegisteredTickets();
    int getRegisteredCount() const { return m_registeredTickets.size(); }
    bool isTicketRegistered(int ticketId) const { return m_registeredTickets.contains(ticketId); }
    bool isValidCheckDigit(int ticketId, int checkDigit) const;
    QString getFormattedBarcode(int ticketId) const;
    QSet<int> getRegisteredTickets() const { return m_registeredTickets; }
    QVector<int> getTicketNumbers(int baseId, int ticketId) const;

    // Processa um numero sorteado
    // Retorna true se houver novidades (novos ganhadores ou armados)
    bool processNumber(int number);

    // Cancela a ultima bola sorteada
    int undoLastNumber(const QSet<int> &preRealizedIds = {});

    // Gestão de Premiações
    void addPrize(const Prize &prize);
    void clearPrizes();
    void setPrizeStatus(int id, bool realizada);
    QList<Prize> getPrizes() const { return m_prizes; }

    // Getters para estado atual
    QList<int> getDrawnNumbers() const;
    QList<int> getWinners() const; // IDs das cartelas ganhadoras (BINGO/Cheia)
    QMap<int, QList<int>> getNearWinTickets() const; // Map: Falta 1 -> [IDs], Falta 2 -> [IDs]...
    QJsonObject getDebugReport() const;

private:
    QMap<int, QVector<BingoTicket>> m_bases; // baseId -> Tickets
    int m_currentGridIndex; 
    int m_maxBalls;         
    int m_numChances;       
    
    QList<int> m_drawnNumbers; 
    
    // Estado de cada cartela: Map (baseId, ticketId) -> State
    QMap<QPair<int, int>, TicketState> m_activeTickets;

    // Cache de vencedores e armados
    QList<int> m_winners;
    QMap<int, QList<int>> m_nearWins; 

    QSet<int> m_registeredTickets; 
    QMap<int, QHash<int, int>> m_idToDigitByBase;   // baseId -> (ID -> Digito)
    QMap<int, QHash<int, QList<int>>> m_numToTicketsByBase; // baseId -> (Número -> Tickets)
    QList<Prize> m_prizes;         
};

#endif // BINGOGAMEENGINE_H
