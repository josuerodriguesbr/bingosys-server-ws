#include "BingoGameEngine.h"
#include <QDebug>

BingoGameEngine::BingoGameEngine(QObject *parent) 
    : QObject(parent), m_currentGridIndex(0), m_maxBalls(75), m_numChances(1)
{
}

void BingoGameEngine::loadTickets(const QVector<BingoTicket> &tickets)
{
    m_allTickets = tickets;
    m_idToDigit.clear();
    for(const auto& t : m_allTickets) {
        m_idToDigit.insert(t.id, t.checkDigit);
    }
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

    // OTIMIZAÇÃO: Se temos cartelas registradas, iteramos APENAS sobre elas.
    // Isso é muito mais rápido do que iterar sobre os 10 milhões de cartelas totais.
    if (!m_registeredTickets.isEmpty()) {
        for (int ticketId : m_registeredTickets) {
            // No sistema, IDs são 1-based, m_allTickets é 0-indexed
            int idx = ticketId - 1;
            if (idx < 0 || idx >= m_allTickets.size()) continue;

            const auto &ticket = m_allTickets[idx];
            if (m_currentGridIndex >= ticket.grids.size()) continue;

            TicketState state;
            state.ticketId = ticket.id;
            state.matches = 0;
            const QVector<int> &grid = ticket.grids[m_currentGridIndex];
            state.totalNumbers = grid.size();
            for(int n : grid) {
                state.missingNumbers.insert(n);
            }
            m_activeTickets.insert(ticket.id, state);
        }
    } else {
        // Se NÃO há registro de vendas (modo treino ou livre), usamos todas as cartelas carregadas.
        for (const auto &ticket : m_allTickets) {
            if (m_currentGridIndex >= ticket.grids.size()) continue;

            TicketState state;
            state.ticketId = ticket.id;
            state.matches = 0;
            const QVector<int> &grid = ticket.grids[m_currentGridIndex];
            state.totalNumbers = grid.size();
            for(int n : grid) {
                state.missingNumbers.insert(n);
            }
            m_activeTickets.insert(ticket.id, state);
        }
    }

    qInfo() << "Novo sorteio iniciado com" << m_activeTickets.size() << "cartelas ativas.";
}

bool BingoGameEngine::processNumber(int number)
{
    if (number < 1 || number > m_maxBalls) {
        qWarning() << "GameEngine: Numero invalido ignorado:" << number << "(Max:" << m_maxBalls << ")";
        return false;
    }

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

int BingoGameEngine::undoLastNumber()
{
    if (m_drawnNumbers.isEmpty()) {
        return -1;
    }

    int lastNum = m_drawnNumbers.takeLast();

    // Itera sobre as cartelas ativas para desfazer o ponto
    for (auto it = m_activeTickets.begin(); it != m_activeTickets.end(); ++it) {
        TicketState &state = it.value();

        // Verifica se esta cartela possui o numero que estamos cancelando
        // Precisamos verificar na m_allTickets original se o numero existe naquela grade
        // Mas podemos simplesmente ver se o numero NÃO está no state.missingNumbers
        // E se ele faz parte da grade original. 
        // Como o load de tickets é estático, podemos buscar na m_allTickets:
        const QVector<int> &grid = m_allTickets[state.ticketId - 1].grids[m_currentGridIndex];
        
        if (grid.contains(lastNum)) {
            // Se o numero estava na grade e não está no missing, significa que foi marcado
            if (!state.missingNumbers.contains(lastNum)) {
                state.missingNumbers.insert(lastNum);
                int oldMatches = state.matches;
                state.matches--;
                
                int newMissingCount = state.missingNumbers.size(); // que é matches_originais+1? nao, total-matches
                
                // Se ela era vencedora, remove de winners
                if (oldMatches == state.totalNumbers) {
                    m_winners.removeAll(state.ticketId);
                }

                // Atualiza nearWins
                // Se agora falta 1, ela veio do Winner.
                // Se agora falta 2, ela veio do bucket 1.
                // Se agora falta 3, ela veio do bucket 2.
                // Se agora falta 4, ela sai do bucket 3.
                
                if (newMissingCount == 1) {
                    m_nearWins[1].append(state.ticketId);
                } else if (newMissingCount <= 3) {
                    m_nearWins[newMissingCount - 1].removeAll(state.ticketId);
                    m_nearWins[newMissingCount].append(state.ticketId);
                } else if (newMissingCount == 4) {
                    m_nearWins[3].removeAll(state.ticketId);
                }
            }
        }
    }

    return lastNum;
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

void BingoGameEngine::registerTicket(int ticketId)
{
    m_registeredTickets.insert(ticketId);
}

void BingoGameEngine::unregisterTicket(int ticketId)
{
    m_registeredTickets.remove(ticketId);
}

void BingoGameEngine::clearRegisteredTickets()
{
    m_registeredTickets.clear();
}

QString BingoGameEngine::getFormattedBarcode(int ticketId) const
{
    int digit = m_idToDigit.value(ticketId, 0);
    // Formato: 6 digitos para ID + 1 digito para CD = 7 total
    return QString("%1%2").arg(ticketId, 6, 10, QChar('0')).arg(digit);
}

bool BingoGameEngine::isValidCheckDigit(int ticketId, int checkDigit) const
{
    return m_idToDigit.contains(ticketId) && m_idToDigit.value(ticketId) == checkDigit;
}

QVector<int> BingoGameEngine::getTicketNumbers(int ticketId) const
{
    int idx = ticketId - 1;
    if (idx < 0 || idx >= m_allTickets.size()) return {};
    
    const auto &ticket = m_allTickets[idx];
    if (m_currentGridIndex < 0 || m_currentGridIndex >= ticket.grids.size()) return {};
    
    return ticket.grids[m_currentGridIndex];
}
