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
    // Limpa ganhadores de cada prêmio
    for(auto &p : m_prizes) p.winners.clear();
    
    // Re-ativa as cartelas já registradas com estado limpo
    setGameMode(m_currentGridIndex); 
}

bool BingoGameEngine::processNumber(int number)
{
    if (number < 1 || number > m_maxBalls) return false;
    if (m_drawnNumbers.contains(number)) return false;

    m_drawnNumbers.append(number);
    bool hasUpdates = false;

    const QList<int> &targets = m_numToTickets[number];
    for (int ticketId : targets) {
        if (!m_activeTickets.contains(ticketId)) continue;
        
        TicketState &state = m_activeTickets[ticketId];
        if (state.missingNumbers.contains(number)) {
            state.missingNumbers.remove(number);
            state.matches++;
            
            // 1. Verificação de Prêmios Customizados (Quinas, Formas, etc)
            for (auto &prize : m_prizes) {
                if (!prize.active) continue;
                // if (prize.winners.contains(ticketId)) continue; // Comentado para permitir debug melhor

                bool won = false;
                bool nearWin = false; // "Boa"
                
                const auto &grid = m_allTickets[ticketId-1].grids[m_currentGridIndex];

                if (prize.tipo == "quina") {
                    // Verificação Inteligente de Quinas 
                    // Assume 5 colunas por padrão
                    int cols = 5;
                    int rows = grid.size() / cols;
                    
                    // Horizontal (Check missing count per line)
                    for (int r = 0; r < rows; ++r) {
                        int missingInLine = 0;
                        for (int c = 0; c < cols; ++c) {
                            if (!m_drawnNumbers.contains(grid[r * cols + c])) missingInLine++;
                        }
                        if (missingInLine == 0) { won = true; break; }
                        if (missingInLine == 1) nearWin = true; 
                    }
                    
                    if (!won) { // Se não ganhou na horizontal, tenta vertical/diagonal
                        // Vertical (Se rows >= 5)
                        if (rows >= 5) {
                            for (int c = 0; c < cols; ++c) {
                                int missingInCol = 0;
                                for (int r = 0; r < rows; ++r) {
                                    if (!m_drawnNumbers.contains(grid[r * cols + c])) missingInCol++;
                                }
                                if (missingInCol == 0) { won = true; break; }
                                if (missingInCol == 1) nearWin = true;
                            }
                        }

                        // Diagonais (Se quadrado 5x5)
                        if (!won && rows == 5 && cols == 5) {
                            int mD1 = 0, mD2 = 0;
                            for (int i = 0; i < 5; ++i) {
                                if (!m_drawnNumbers.contains(grid[i * 6])) mD1++;
                                if (!m_drawnNumbers.contains(grid[(i + 1) * 4])) mD2++;
                            }
                            if (mD1 == 0 || mD2 == 0) won = true;
                            if (mD1 == 1 || mD2 == 1) nearWin = true;
                        }
                    }

                } else if (prize.tipo == "forma" || prize.tipo == "cheia") {
                    // Lógica de Padrão ou Cartela Cheia
                    int missingCount = 0;
                    
                    if (prize.tipo == "cheia") {
                        missingCount = state.missingNumbers.size();
                    } else {
                        // Forma customizada
                        // Se padraoIndices vazio, não vence (evita vitória automática)
                        if (prize.padraoIndices.isEmpty()) { 
                            won = false; 
                        } else {
                           for (int idx : prize.padraoIndices) {
                               if (idx >= 0 && idx < grid.size()) {
                                   if (!m_drawnNumbers.contains(grid[idx])) missingCount++;
                               }
                           }
                        }
                    }
                    
                    if (missingCount == 0 && (prize.tipo == "cheia" || !prize.padraoIndices.isEmpty())) won = true;
                    if (missingCount == 1) nearWin = true;
                }

                // Atualiza listas de vencedores e 'boas'
                if (won) {
                    if (!prize.winners.contains(ticketId)) {
                        prize.winners.append(ticketId);
                        // Se ganhou, não está mais 'armada'
                        prize.near_winners.removeOne(ticketId);
                        hasUpdates = true;
                    }
                } else if (nearWin) {
                    if (!prize.near_winners.contains(ticketId) && !prize.winners.contains(ticketId)) {
                        prize.near_winners.append(ticketId);
                        hasUpdates = true;
                    }
                } else {
                    // Nem ganhou, nem boa -> remove de boa (caso estivesse)
                    if (prize.near_winners.removeOne(ticketId)) hasUpdates = true;
                }
            }

            // 2. Verificação de Cartela Cheia GLOBAL (BINGO) - Mantendo lógica antiga por compatibilidade
            int missingCount = state.missingNumbers.size();
            if (missingCount == 0) {
                if (!m_winners.contains(state.ticketId)) {
                    m_winners.append(state.ticketId);
                    for(int i=1; i<=3; ++i) m_nearWins[i].removeAll(state.ticketId);
                    hasUpdates = true; 
                    qInfo() << "BINGO! Ticket" << state.ticketId << "venceu cartela cheia.";
                }
            } else if (missingCount <= 3) {
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
    m_prizes.append(prize);
}

void BingoGameEngine::clearPrizes()
{
    m_prizes.clear();
}
