#include "BingoGameEngine.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

BingoGameEngine::BingoGameEngine(QObject *parent) 
    : QObject(parent), m_currentGridIndex(0), m_maxBalls(75), m_numChances(1)
{
}

void BingoGameEngine::loadBase(int baseId, const QVector<BingoTicket> &tickets)
{
    m_bases[baseId] = tickets;
    m_idToDigitByBase[baseId].clear();
    for(const auto& t : tickets) {
        m_idToDigitByBase[baseId].insert(t.id, t.checkDigit);
    }
    qInfo() << "GameEngine: Carregada base" << baseId << "com" << tickets.size() << "cartelas.";
}

void BingoGameEngine::setGameMode(int gridIndex)
{
    m_currentGridIndex = gridIndex;
    m_activeTickets.clear();
    m_winners.clear();
    m_nearWins.clear();
    m_numToTicketsByBase.clear();

    if (m_registeredTickets.isEmpty()) return;

    // Inicializa estados para todas as combinações de (Base x Ticket Registrado)
    // conforme os prêmios cadastrados
    for (const auto &prize : m_prizes) {
        int bid = prize.baseId;
        int gidx = prize.gridIndex;
        if (!m_bases.contains(bid)) continue;
        const auto &tickets = m_bases[bid];

        for (int ticketId : m_registeredTickets) {
            // Verifica se já inicializamos este par (baseId, ticketId)
            QPair<int, int> key(bid, ticketId);
            if (m_activeTickets.contains(key)) continue;

            int idx = ticketId - 1; 
            if (idx < 0 || idx >= tickets.size()) continue;
            const auto &ticket = tickets[idx];
            if (gidx < 0 || gidx >= ticket.grids.size()) continue;

            TicketState state;
            state.ticketId = ticket.id;
            state.baseId = bid;
            state.matches = 0;
            const QVector<int> &grid = ticket.grids[gidx];
            state.totalNumbers = grid.size();
            for(int n : grid) {
                state.missingNumbers.insert(n);
                m_numToTicketsByBase[bid][n].append(ticket.id);
            }
            m_activeTickets.insert(key, state);
        }
    }
    qInfo() << "GameEngine: Estados ativos inicializados para" << m_activeTickets.size() << "combinações base/cartela.";
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
        // 1. Atualização de estados em todas as bases que contêm o número
        for (auto it = m_numToTicketsByBase.begin(); it != m_numToTicketsByBase.end(); ++it) {
            int bid = it.key();
            const auto &targets = it.value()[number];
            for (int ticketId : targets) {
                QPair<int, int> key(bid, ticketId);
                if (!m_activeTickets.contains(key)) continue;
                TicketState &state = m_activeTickets[key];
                if (state.missingNumbers.contains(number)) {
                    state.missingNumbers.remove(number);
                    state.matches++;
                }
            }
        }
    }

    // 2. Verificação de Prêmios para TODAS as cartelas ativas (Devido a Turno Global + Formas Paralelas)
        for (auto it = m_activeTickets.begin(); it != m_activeTickets.end(); ++it) {
            TicketState &state = it.value();
            if (state.usedPatterns.contains("cheia")) continue; 

            for (auto &prize : m_prizes) {
                if (!prize.active || prize.realizada) continue;
                if (prize.baseId != state.baseId) continue; // Só avalia se for a base do prêmio

                bool eligible = (prize.tipo == "forma") || (sequentialTurnId != -1 && prize.id == sequentialTurnId);
                if (!eligible) continue;

                bool won = false;
                bool nearWin = false;
                const auto &tickets = m_bases[prize.baseId];
                const auto &grid = tickets[state.ticketId-1].grids[prize.gridIndex];

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
                        if (!m_winners.contains(state.ticketId)) m_winners.append(state.ticketId);
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

int BingoGameEngine::undoLastNumber(const QSet<int> &preRealizedIds)
{
    if (m_drawnNumbers.isEmpty()) {
        return -1;
    }

    // Estratégia de Reset e Replay: Garante integridade absoluta de ganhadores, armados e turnos
    QList<int> balls = m_drawnNumbers;
    int lastNum = balls.takeLast();
    
    qInfo() << "[UNDO-ENGINE] Iniciando Replay para desfazer a bola" << lastNum;
    
    // 1. Reseta o motor para o estado zero (mantendo configurações e cartelas registradas)
    startNewGame(); 
    
    // 2. Re-processa todas as bolas exceto a última
    for (int n : balls) {
        processNumber(n);
        
        // Verifica se algum prêmio que estava 'realizado' antes do undo já obteve vitoria neste replay.
        // Se sim, precisamos marcá-lo como 'realizada' novamente para avançar o turno no replay.
        for (auto &p : m_prizes) {
            if (preRealizedIds.contains(p.id) && !p.realizada && !p.winners.isEmpty()) {
                p.realizada = true;
                qInfo() << "[UNDO-REPLAY] Restaurando status 'realizada' para prêmio" << p.id << p.nome << "durante replay.";
            }
        }
    }

    qInfo() << "[UNDO-ENGINE] Replay concluído. Bolas restantes:" << m_drawnNumbers.size();
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
    if (m_registeredTickets.contains(ticketId)) return; 
    m_registeredTickets.insert(ticketId);

    // Inicializa estados para este ticket em todas as bases requeridas pelos prêmios
    for (const auto &prize : m_prizes) {
        int bid = prize.baseId;
        int gidx = prize.gridIndex;
        if (!m_bases.contains(bid)) continue;
        
        QPair<int, int> key(bid, ticketId);
        if (m_activeTickets.contains(key)) continue;

        const auto &tickets = m_bases[bid];
        int idx = ticketId - 1;
        if (idx < 0 || idx >= tickets.size()) continue;
        
        const auto &ticket = tickets[idx];
        if (gidx < 0 || gidx >= ticket.grids.size()) continue;

        TicketState state;
        state.ticketId = ticket.id;
        state.baseId = bid;
        state.matches = 0;
        const QVector<int> &grid = ticket.grids[gidx];
        state.totalNumbers = grid.size();
        
        for(int n : grid) {
            if (m_drawnNumbers.contains(n)) {
                state.matches++;
            } else {
                state.missingNumbers.insert(n);
            }
            m_numToTicketsByBase[bid][n].append(ticket.id);
        }
        
        m_activeTickets.insert(key, state);
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
    // Se não especificado base, busca na primeira base que contém o ticket
    int digit = 0;
    for (auto it = m_idToDigitByBase.begin(); it != m_idToDigitByBase.end(); ++it) {
        if (it.value().contains(ticketId)) {
            digit = it.value().value(ticketId);
            break;
        }
    }
    // Formato: 6 digitos para ID + 1 digito para CD = 7 total
    return QString("%1%2").arg(ticketId, 6, 10, QChar('0')).arg(digit);
}

QVector<int> BingoGameEngine::getTicketNumbers(int baseId, int ticketId) const
{
    if (m_bases.contains(baseId)) {
        const auto &tickets = m_bases[baseId];
        int idx = ticketId - 1;
        if (idx >= 0 && idx < tickets.size()) {
            // Retorna a união de todos os números de todas as grades deste ticket nesta base?
            // Ou apenas da grade "atual" (m_currentGridIndex)?
            // Para o visual da cartela, o melhor é retornar os números da grade que o prêmio ativo ou gridIndex define.
            // Aqui vamos retornar a 1ª grade por padrão, ou a que tiver números.
            const auto &grids = tickets[idx].grids;
            if (!grids.isEmpty()) {
                // Se o currentGridIndex for válido para este ticket, usa ele
                if (m_currentGridIndex >= 0 && m_currentGridIndex < grids.size())
                    return grids[m_currentGridIndex];
                return grids.first();
            }
        }
    }
    return {};
}

bool BingoGameEngine::isValidCheckDigit(int ticketId, int checkDigit) const
{
    for (auto it = m_idToDigitByBase.begin(); it != m_idToDigitByBase.end(); ++it) {
        if (it.value().contains(ticketId)) {
            if (it.value().value(ticketId) == checkDigit) return true;
        }
    }
    return false;
}

// Deletado: getTicketNumbers(int ticketId) legado

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
