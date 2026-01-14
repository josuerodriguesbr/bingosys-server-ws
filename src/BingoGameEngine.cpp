#include "BingoGameEngine.h"
#include <QDebug>

BingoGameEngine::BingoGameEngine(QObject *parent) : QObject(parent), m_currentGridIndex(0)
{
}

void BingoGameEngine::loadTickets(const QVector<BingoTicket> &tickets)
{
    m_allTickets = tickets;
    qInfo() << "GameEngine: Carregadas" << m_allTickets.size() << "cartelas na memoria.";
}

void BingoGameEngine::setGameMode(int gridIndex)
{
    m_currentGridIndex = gridIndex;
    qInfo() << "GameEngine: Modo de jogo definido para indice de grade:" << m_currentGridIndex;
}

void BingoGameEngine::startNewGame()
{
    m_drawnNumbers.clear();
    m_activeTickets.clear();
    m_winners.clear();
    m_nearWins.clear();

    // Inicializa o estado das cartelas para o modo de jogo atual
    for (const auto &ticket : m_allTickets) {
        if (m_currentGridIndex >= ticket.grids.size()) {
            qWarning() << "Cartela" << ticket.id << "nao possui grade no indice" << m_currentGridIndex;
            continue;
        }

        TicketState state;
        state.ticketId = ticket.id;
        state.matches = 0;
        
        // Copia os numeros da grade alvo para o set de "missing"
        // Assim podemos remover conforme saem os numeros
        const QVector<int> &grid = ticket.grids[m_currentGridIndex];
        state.totalNumbers = grid.size();
        for(int n : grid) {
            state.missingNumbers.insert(n);
        }

        m_activeTickets.insert(ticket.id, state);
    }

    qInfo() << "Novo sorteio iniciado com" << m_activeTickets.size() << "cartelas ativas.";
}

bool BingoGameEngine::processNumber(int number)
{
    if (m_drawnNumbers.contains(number)) {
        return false; // Numero repetido
    }

    m_drawnNumbers.append(number);
    bool hasUpdates = false;

    // Itera sobre as cartelas ativas para marcar o ponto
    // Otimizacao: Em C++ QMap iterator
    for (auto it = m_activeTickets.begin(); it != m_activeTickets.end(); ++it) {
        TicketState &state = it.value();

        if (state.missingNumbers.contains(number)) {
            state.missingNumbers.remove(number);
            state.matches++;
            
            int missingCount = state.missingNumbers.size();
            
            // Verifica vitoria
            if (missingCount == 0) {
                if (!m_winners.contains(state.ticketId)) {
                    m_winners.append(state.ticketId);
                    // Remove de nearWins se estava la
                    // (Isso requer varrer o nearWins ou saber onde estava, 
                    //  por simplicidade, podemos limpar e reconstruir nearWins ou gerenciar com cuidado)
                    // Simplificacao: O getNearWinTickets reconstroi ou mantemos atualizado.
                    // Vamos manter atualizado:
                    m_nearWins[1].removeAll(state.ticketId); 
                    hasUpdates = true;
                    qInfo() << "BINGO! Cartela" << state.ticketId << "ganhou!";
                }
            } 
            // Verifica se esta "armada" (Faltam 3, 2 ou 1)
            else if (missingCount <= 3) {
                // Remove do bucket anterior (ex: faltava 4, agora 3... ou faltava 2 agora 1)
                // Se faltava 2 (bucket 2), agora falta 1 (bucket 1).
                // Precisamos remover do bucket (missingCount + 1)
                m_nearWins[missingCount + 1].removeAll(state.ticketId);
                
                if (!m_nearWins[missingCount].contains(state.ticketId)) {
                    m_nearWins[missingCount].append(state.ticketId);
                    hasUpdates = true;
                }
            }
        }
    }

    return hasUpdates;
}

QList<int> BingoGameEngine::getDrawnNumbers() const
{
    return m_drawnNumbers;
}

QList<int> BingoGameEngine::getWinners() const
{
    return m_winners;
}

QMap<int, QList<int>> BingoGameEngine::getNearWinTickets() const
{
    return m_nearWins;
}
