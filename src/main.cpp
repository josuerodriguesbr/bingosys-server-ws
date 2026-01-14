#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include "BingoServer.h"
#include "BingoTicketParser.h"
#include "BingoGameEngine.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Porta padrão 3000 ou via argumento
    // Porta padrão 3000 ou via argumento
    quint16 port = 3000;
    
    if (argc > 1) {
        port = QString(argv[1]).toUShort();
    }

    qInfo() << "Iniciando BingoSys Server na porta" << port << "...";

    // --- CARGA DE CARTELAS ---
    // Na VPS o path será relativo ao executável ou no diretório data padrão
    QString dataPath = "data/base-cartelas.TXT"; 
    
    if (argc > 2) {
        dataPath = argv[2];
    }

    qInfo() << "Tentando carregar cartelas de:" << dataPath;
    auto tickets = BingoTicketParser::parseFile(dataPath);
    qInfo() << "Cartelas carregadas:" << tickets.size();
    
    if (!tickets.isEmpty()) {
        const auto &t = tickets.first();
        qInfo() << "Exemplo Cartela ID:" << t.id << "DV:" << t.checkDigit 
                << "Qtd Grades:" << t.grids.size();
        if(!t.grids.isEmpty()) {
            qInfo() << "Grade 0 (Ex: 15x60) Qtd Dezenas:" << t.grids[0].size();
        }
    }
    // --- TESTE DE GAME ENGINE ---
    qInfo() << "--- Testando Game Engine ---";
    BingoGameEngine engine;
    engine.loadTickets(tickets);
    
    // Configura modo 25x75 (supondo que seja o índice 3 conforme sua descricao: 15x60, 20x60, 15x75, 25x75)
    // Mas vamos usar indice 0 (1a parte) pq eh o que temos certeza do formato 0000019-0110...
    engine.setGameMode(0); 
    engine.startNewGame();
    
    // Simula sorteio de alguns numeros que sabemos que existem na primeira cartela
    // Cartela 1 parte 1: 01 10 12 15 19 22 34 35 36 37 42 46 53 55 58
    QList<int> testDraws = {1, 10, 12, 15, 19, 22, 34, 35, 36, 37, 42, 46, 53, 55}; // Falta o 58
    
    for(int n : testDraws) {
        engine.processNumber(n);
    }
    
    auto nearWins = engine.getNearWinTickets();
    qInfo() << "Cartelas faltando 1 (devem incluir ID 1):" << nearWins[1];
    
    // Sorteia o ultimo
    engine.processNumber(58);
    qInfo() << "Vencedores após sortear 58:" << engine.getWinners();
    // ----------------------------

    BingoServer server(port);
    server.loadTickets(tickets); // Passa as cartelas carregadas para o servidor
    
    // Configura persistencia de vendas
    QString salesPath = QFileInfo(dataPath).absolutePath() + "/sold-tickets.TXT";
    server.setSalesPath(salesPath);
    server.loadSales();
    
    if (!server.start()) {
        qCritical() << "Falha ao iniciar o servidor na porta" << port;
        return 1;
    }

    qInfo() << "Servidor rodando e aguardando conexoes na porta" << port;

    return a.exec();
}
