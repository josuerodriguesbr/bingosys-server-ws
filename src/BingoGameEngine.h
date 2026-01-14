#ifndef BINGOGAMEENGINE_H
#define BINGOGAMEENGINE_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QMap>
#include "BingoTicketParser.h"

struct TicketState {
    int ticketId;
    int matches; // Quantidade de acertos
    QSet<int> missingNumbers; // Numeros que faltam (opcional, para saber quais faltam)
    // Ou apenas manter count se a performance for critica
    int totalNumbers;
};

class BingoGameEngine : public QObject
{
    Q_OBJECT
public:
    explicit BingoGameEngine(QObject *parent = nullptr);

    // Carrega as cartelas para o jogo
    void loadTickets(const QVector<BingoTicket> &tickets);

    // Configura qual "grid" usar (0 = 1ª parte, 1 = 2ª parte...)
    // Isso define o modo de jogo (ex: 15x60 ou 25x75)
    void setGameMode(int gridIndex);

    // Inicia um novo sorteio (limpa estado)
    void startNewGame();

    // Processa um numero sorteado
    // Retorna true se houver novidades (novos ganhadores ou armados)
    bool processNumber(int number);

    // Getters para estado atual
    QList<int> getDrawnNumbers() const;
    QList<int> getWinners() const; // IDs das cartelas ganhadoras
    QMap<int, QList<int>> getNearWinTickets() const; // Map: Falta 1 -> [IDs], Falta 2 -> [IDs]...

private:
    QVector<BingoTicket> m_allTickets; // Todas as cartelas carregadas
    int m_currentGridIndex; // Qual grade estamos jogando
    
    QList<int> m_drawnNumbers; // Histórico do sorteio atual
    
    // Estado de cada cartela jogando: Map ID -> State
    QMap<int, TicketState> m_activeTickets;

    // Cache de vencedores e armados para acesso rapido
    QList<int> m_winners;
    QMap<int, QList<int>> m_nearWins; // Key: 1 (falta 1), 2 (falta 2), etc.
};

#endif // BINGOGAMEENGINE_H
