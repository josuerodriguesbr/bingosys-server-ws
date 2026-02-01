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

    // --- CARGA DE CARTELAS MESTRE ---
    QString dataPath = "data/base-cartelas.TXT"; 
    if (argc > 2) dataPath = argv[2];

    qInfo() << "Carregando base de cartelas:" << dataPath;
    auto tickets = BingoTicketParser::parseFile(dataPath);
    qInfo() << "Total de cartelas mestre:" << tickets.size();

    if (tickets.isEmpty()) {
        qCritical() << "ERRO: Nenhuma cartela carregada. Verifique o arquivo de dados.";
        return 1;
    }

    // Inicializa o servidor que agora gerencia DB e Sorteios
    BingoServer server(port);
    server.loadTickets(tickets); // Cartelas usadas como base para todos os jogos
    
    if (!server.start()) {
        qCritical() << "Falha ao iniciar o servidor na porta" << port;
        return 1;
    }

    qInfo() << "BingoSys (Módulo Geral) rodando na porta" << port;

    return a.exec();
}
