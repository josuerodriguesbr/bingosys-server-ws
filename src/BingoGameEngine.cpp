#include "BingoGameEngine.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

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
    m_activeTickets.clear();
    m_winners.clear();
    m_nearWins.clear();
    m_numToTickets.clear();

    // Se houver cartelas registradas, usamos apenas elas.
    // Se não, o sorteio fica vazio até que vendas sejam importadas/registradas.
    if (!m_registeredTickets.isEmpty()) {
        for (int ticketId : m_registeredTickets) {
            int idx = ticketId - 1;
            if (idx < 0 || idx >= m_allTickets.size()) continue;
            const auto &ticket = m_allTickets[idx];
            if (m_currentGridIndex < 0 || m_currentGridIndex >= ticket.grids.size()) continue;

            TicketState state;
            state.ticketId = ticket.id;
            state.matches = 0;
            const QVector<int> &grid = ticket.grids[m_currentGridIndex];
            state.totalNumbers = grid.size();
            for(int n : grid) {
                state.missingNumbers.insert(n);
                m_numToTickets[n].append(ticket.id);
            }
            m_activeTickets.insert(ticket.id, state);
        }
    }
    qInfo() << "GameEngine: Modo definido e" << m_activeTickets.size() << "cartelas ativas prontas.";
}

void BingoGameEngine::startNewGame()
{
    qInfo() << "GameEngine: Reiniciando sorteio (limpando bolas e estado)...";
    m_drawnNumbers.clear();
    m_winners.clear();
    m_nearWins.clear();
    // Limpa ganhadores de cada prêmio e reseta a flag realizada
    for(auto &p : m_prizes) {
        p.winners.clear();
        p.realizada = false;
        p.winnerPatterns.clear();
    }
    
    // Re-ativa as cartelas já registradas com estado limpo
    setGameMode(m_currentGridIndex); 
}

bool BingoGameEngine::processNumber(int number)
{
    if (number != 0) {
        if (number < 1 || number > m_maxBalls) return false;
        if (m_drawnNumbers.contains(number)) return false;
        m_drawnNumbers.append(number);
    }

    bool hasUpdates = false;

    // Sistema de Turnos SEQUENCIAL: Identifica o próximo prêmio que NÃO é do tipo "forma"
    int sequentialTurnId = -1;
    for (const auto &p : m_prizes) {
        // Agora comparamos sempre em lowercase (normalizado em addPrize)
        if (p.active && !p.realizada && p.tipo != "forma") {
            sequentialTurnId = p.id;
            break; 
        }
    }
    
    if (number != 0) {
        qDebug() << "GameEngine: processNumber" << number << "Turno Sequencial ID:" << sequentialTurnId;
        // 1. Atualização rápida de estados (apenas cartelas com o número sorteado)
        const QList<int> &targets = m_numToTickets[number];
        for (int ticketId : targets) {
            if (!m_activeTickets.contains(ticketId)) continue;
            TicketState &state = m_activeTickets[ticketId];
            if (state.missingNumbers.contains(number)) {
                state.missingNumbers.remove(number);
                state.matches++;
            }
        }
    }

    // 2. Verificação de Prêmios para TODAS as cartelas ativas (Devido a Turno Global + Formas Paralelas)
    for (auto &state : m_activeTickets) {
        if (state.usedPatterns.contains("cheia")) continue; // Cartela cheia já ganhou o prêmio máximo

        for (auto &prize : m_prizes) {
            if (!prize.active || prize.realizada) continue;
            
            // REGRA DE ELEGIBILIDADE:
            // - Prêmios do tipo "forma" são processados SEMPRE em paralelo (independente de turno).
            // - Outros prêmios (Quinas, Cheia) seguem a ordem do Turno Sequencial.
            bool eligible = (prize.tipo == "forma") || (sequentialTurnId != -1 && prize.id == sequentialTurnId);
            if (!eligible) continue;

            bool won = false;
            bool nearWin = false;
            const auto &grid = m_allTickets[state.ticketId-1].grids[m_currentGridIndex];

            if (prize.tipo == "quina") {
                int cols = 5;
                int rows = grid.size() / cols;
                for (int r = 0; r < rows; ++r) {
                    int miss = 0;
                    QList<int> idxs;
                    for (int c = 0; c < cols; ++c) {
                        int idx = c * rows + r;
                        idxs.append(idx);
                        if (!m_drawnNumbers.contains(grid[idx])) miss++;
                    }
                    if (miss == 0) {
                        QList<int> sorted = idxs;
                        std::sort(sorted.begin(), sorted.end());
                        if (!state.usedPatterns["quina"].contains(sorted)) {
                            won = true;
                            prize.winnerPatterns[state.ticketId] = idxs;
                            break;
                        }
                    }
                    if (miss == 1) nearWin = true;
                }
                
                if (!won && rows == 5 && cols == 5) {
                    // Vertical
                    for (int c = 0; c < cols; ++c) {
                        int miss = 0;
                        QList<int> idxs;
                        for (int r = 0; r < rows; ++r) {
                            int idx = c * rows + r;
                            idxs.append(idx);
                            if (!m_drawnNumbers.contains(grid[idx])) miss++;
                        }
                        if (miss == 0) {
                            QList<int> sorted = idxs;
                            std::sort(sorted.begin(), sorted.end());
                            if (!state.usedPatterns["quina"].contains(sorted)) {
                                won = true;
                                prize.winnerPatterns[state.ticketId] = idxs;
                                break;
                            }
                        }
                        if (miss == 1) nearWin = true;
                    }
                    // Diagonais
                    if (!won) {
                        int m1=0, m2=0;
                        QList<int> i1, i2;
                        for(int i=0; i<5; i++) {
                            int id1 = i*5+i; i1 << id1; if(!m_drawnNumbers.contains(grid[id1])) m1++;
                            int id2 = i*5+(4-i); i2 << id2; if(!m_drawnNumbers.contains(grid[id2])) m2++;
                        }
                        if (m1==0) { 
                            QList<int> s=i1; std::sort(s.begin(), s.end());
                            if(!state.usedPatterns["quina"].contains(s)) { won=true; prize.winnerPatterns[state.ticketId]=i1; }
                        }
                        if (!won && m2==0) {
                            QList<int> s=i2; std::sort(s.begin(), s.end());
                            if(!state.usedPatterns["quina"].contains(s)) { won=true; prize.winnerPatterns[state.ticketId]=i2; }
                        }
                        if (m1==1 || m2==1) nearWin = true;
                    }
                }
            } else if (prize.tipo == "forma" || prize.tipo == "cheia") {
                int missingCount = 0;
                if (prize.tipo == "cheia") {
                    missingCount = state.missingNumbers.size();
                } else {
                    for (int idx : prize.padraoIndices) {
                        if (idx >= 0 && idx < grid.size()) {
                            if (!m_drawnNumbers.contains(grid[idx])) missingCount++;
                        }
                    }
                }
                if (missingCount == 0 && (prize.tipo == "cheia" || !prize.padraoIndices.isEmpty())) won = true;
                if (missingCount == 1) nearWin = true;
            }

            if (won) {
                if (!prize.winners.contains(state.ticketId)) {
                    prize.winners.append(state.ticketId);
                    prize.near_winners.removeOne(state.ticketId);
                    state.wonPrizeIds.insert(prize.id);
                    if (prize.tipo == "quina") {
                        QList<int> s = prize.winnerPatterns[state.ticketId]; std::sort(s.begin(), s.end());
                        state.usedPatterns["quina"].append(s);
                    } else if (prize.tipo == "forma") {
                        QList<int> s = prize.padraoIndices.toList(); std::sort(s.begin(), s.end());
                        state.usedPatterns["forma"].append(s);
                    } else if (prize.tipo == "cheia") {
                        state.usedPatterns["cheia"].append(QList<int>());
                    }
                    qInfo() << "VITÓRIA! Ticket" << state.ticketId << "ganhou prêmio paralalelo/da vez:" << prize.id << prize.nome;
                    hasUpdates = true;
                }
            } else if (nearWin) {
                if (!prize.near_winners.contains(state.ticketId) && !prize.winners.contains(state.ticketId)) {
                    prize.near_winners.append(state.ticketId);
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

    // Otimização: Apenas cartelas que possuem este numero
    const QList<int> &targets = m_numToTickets[lastNum];

    for (int ticketId : targets) {
        if (!m_activeTickets.contains(ticketId)) continue;
        TicketState &state = m_activeTickets[ticketId];

        // Se o numero estava marcado (não estava no missing)
        if (!state.missingNumbers.contains(lastNum)) {
            state.missingNumbers.insert(lastNum);
            int oldMatches = state.matches;
            state.matches--;
            
            int newMissingCount = state.missingNumbers.size();
            
            // 1. Desfazer BINGO Global (Cartela Cheia)
            if (oldMatches == state.totalNumbers) {
                m_winners.removeAll(ticketId);
                state.usedPatterns.remove("cheia"); // FIX: Permite ganhar cheia novamente
            }

            if (newMissingCount == 1) {
                m_nearWins[1].append(ticketId);
            } else if (newMissingCount <= 3) {
                m_nearWins[newMissingCount - 1].removeAll(ticketId);
                if (!m_nearWins[newMissingCount].contains(ticketId))
                    m_nearWins[newMissingCount].append(ticketId);
            } else if (newMissingCount == 4) {
                m_nearWins[3].removeAll(ticketId);
            }

            // 2. Desfazer impacto nos Prêmios Customizados
            const auto &grid = m_allTickets[ticketId - 1].grids[m_currentGridIndex];
            
            for (int pIdx = 0; pIdx < m_prizes.size(); ++pIdx) {
                Prize &prize = m_prizes[pIdx];
                if (!prize.active) continue;

                bool stillWon = false;
                bool nearWin = false;
                int minMissing = 99;

                if (prize.tipo == "quina") {
                    int cols = 5;
                    int rows = grid.size() / cols;
                    
                    // Horizontal
                    for (int r = 0; r < rows; ++r) {
                        int rowMiss = 0;
                        for (int c = 0; c < cols; ++c) {
                            if (!m_drawnNumbers.contains(grid[c * rows + r])) rowMiss++;
                        }
                        if (rowMiss < minMissing) minMissing = rowMiss;
                    }
                    // Vertical e Diagonais (SOMENTE if 5x5)
                    if (rows == 5 && cols == 5) {
                        // Vertical
                        for (int c = 0; c < cols; ++c) {
                            int colMiss = 0;
                            for (int r = 0; r < rows; ++r) {
                                if (!m_drawnNumbers.contains(grid[c * rows + r])) colMiss++;
                            }
                            if (colMiss < minMissing) minMissing = colMiss;
                        }
                        // Diagonais
                        int d1 = 0, d2 = 0;
                        for(int i=0; i<5; ++i) {
                            if (!m_drawnNumbers.contains(grid[i*5 + i])) d1++;
                            if (!m_drawnNumbers.contains(grid[i*5 + (4-i)])) d2++;
                        }
                        if (d1 < minMissing) minMissing = d1;
                        if (d2 < minMissing) minMissing = d2;
                    }
                    
                    if (minMissing == 0) stillWon = true;
                    else if (minMissing == 1) nearWin = true;

                } else if (prize.tipo == "forma" || prize.tipo == "cheia") {
                    int prizeMissing = 0;
                    if (prize.tipo == "cheia") {
                        prizeMissing = state.missingNumbers.size();
                    } else {
                        if (prize.padraoIndices.isEmpty()) prizeMissing = 99;
                        else {
                            for (int idx : prize.padraoIndices) {
                                if (idx >= 0 && idx < grid.size()) {
                                    if (!m_drawnNumbers.contains(grid[idx])) prizeMissing++;
                                }
                            }
                        }
                    }
                    if (prizeMissing == 0 && (prize.tipo == "cheia" || !prize.padraoIndices.isEmpty())) stillWon = true;
                    else if (prizeMissing == 1) nearWin = true;
                }

                // Aplica mudanças no estado do prêmio e da cartela
                if (!stillWon) {
                    qInfo() << "[UNDO-DETAIL] Ticket" << ticketId << "deixou de meet criteria para prêmio" << prize.id << prize.nome << ". Removendo...";
                    
                    // SE NÃO GANHOU MAIS: Limpa o registro de vitória interna da cartela para este prêmio
                    state.wonPrizeIds.remove(prize.id);
                    int removedCount = prize.winners.removeAll(ticketId);
                    prize.winnerPatterns.remove(ticketId);

                    qInfo() << "[UNDO-DETAIL] Ticket" << ticketId << "removido de prize.winners. Ocorrências removidas:" << removedCount;

                    // Limpa padrões usados especificamente para este prêmio/tipo
                    if (prize.tipo == "quina") {
                         state.usedPatterns.remove("quina");
                    } else if (prize.tipo == "forma") {
                         state.usedPatterns.remove("forma");
                    } else if (prize.tipo == "cheia") {
                         state.usedPatterns.remove("cheia");
                    }
                    
                    qInfo() << "[UNDO-ENGINE] Ticket" << ticketId << "deixou de ganhar prêmio" << prize.id << "(" << prize.nome << ")";
                } else {
                    qDebug() << "[UNDO-DETAIL] Ticket" << ticketId << "ainda ganha prêmio" << prize.id << "com as bolas restantes.";
                }

                if (nearWin && !stillWon) {
                    if (!prize.near_winners.contains(ticketId)) prize.near_winners.append(ticketId);
                } else {
                    prize.near_winners.removeAll(ticketId);
                }
            }
        }
    }
    
    qInfo() << "[UNDO-ENGINE] Sorteio atualizado. Última bola removida:" << lastNum << ". Restantes:" << m_drawnNumbers.size();
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
    if (m_registeredTickets.contains(ticketId)) return; // Já registrada
    m_registeredTickets.insert(ticketId);

    // Inicializa a cartela para o jogo atual
    int idx = ticketId - 1;
    if (idx < 0 || idx >= m_allTickets.size()) return;
    
    const auto &ticket = m_allTickets[idx];
    if (m_currentGridIndex < 0 || m_currentGridIndex >= ticket.grids.size()) return;

    if (!m_activeTickets.contains(ticketId)) {
        TicketState state;
        state.ticketId = ticket.id;
        state.matches = 0;
        const QVector<int> &grid = ticket.grids[m_currentGridIndex];
        state.totalNumbers = grid.size();
        
        for(int n : grid) {
            // Se o numero já foi sorteado, já conta como marcado!
            if (m_drawnNumbers.contains(n)) {
                state.matches++;
            } else {
                state.missingNumbers.insert(n);
            }
            m_numToTickets[n].append(ticket.id);
        }
        
        m_activeTickets.insert(ticket.id, state);
        // qDebug() << "Ticket" << ticketId << "ativado tardiamente com" << state.matches << "matches iniciais.";
    }
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

void BingoGameEngine::addPrize(const Prize &prize)
{
    Prize p = prize;
    p.tipo = p.tipo.toLower(); // Normaliza para evitar problemas de case (forma vs FORMA)
    m_prizes.append(p);
}

void BingoGameEngine::clearPrizes()
{
    m_prizes.clear();
}

void BingoGameEngine::setPrizeStatus(int id, bool realizada)
{
    for(auto &p : m_prizes) {
        if (p.id == id) {
            p.realizada = realizada;
            // Ao finalizar um prêmio, precisamos forçar uma verificação do ENGINE
            // pois o PRÓXIMO prêmio na vez pode já ter ganhadores (ganhos retroativos)
            qInfo() << "GameEngine: Prêmio" << id << "finalizado. Forçando check do próximo turno...";
            processNumber(0); 
            break;
        }
    }
}

QJsonObject BingoGameEngine::getDebugReport() const {
    QJsonObject report;
    QJsonArray prizesReport;
    
    // Sistema de Turnos GLOBAL: Identifica qual prêmio está valendo agora
    int premioDaVezId = -1;
    for (const auto &p : m_prizes) {
        if (p.active && !p.realizada) {
            premioDaVezId = p.id;
            break;
        }
    }

    for (const auto &p : m_prizes) {
        QJsonObject po;
        po["id"] = p.id;
        po["nome"] = p.nome;
        po["tipo"] = p.tipo;
        po["active"] = p.active;
        po["realizada"] = p.realizada;
        po["isTurnPrize"] = (p.id == premioDaVezId);
        po["winnersCount"] = p.winners.size();
        
        QJsonArray padrao;
        for(int idx : p.padraoIndices) padrao.append(idx);
        po["padrao"] = padrao;
        
        prizesReport.append(po);
    }
    
    report["prizes"] = prizesReport;
    report["drawnCount"] = m_drawnNumbers.size();
    report["activeTicketsCount"] = m_activeTickets.size();
    report["gridIndex"] = m_currentGridIndex;
    
    return report;
}
