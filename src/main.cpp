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
    
    // Suporte a ferramenta de banco de dados nativa
    if (a.arguments().contains("--exec-sql")) {
        int idx = a.arguments().indexOf("--exec-sql");
        if (a.arguments().size() > idx + 1) {
            QString sqlPath = a.arguments().at(idx + 1);
            BingoDatabaseManager db;
            if (db.connectToDatabase("localhost", "bingosys", "bingosys", "bingosys")) {
                qInfo() << "Executando script SQL local:" << sqlPath;
                bool ok = db.executarScriptSQL(sqlPath);
                return ok ? 0 : 1;
            } else {
                qCritical() << "Falha ao conectar ao banco remoto.";
                return 1;
            }
        } else {
            qCritical() << "Uso: BingoSysServer --exec-sql <caminho_do_arquivo.sql>";
            return 1;
        }
    }

    // Inicializa o servidor que agora gerencia DB e Sorteios
    BingoServer server(port);
    
    if (!server.start()) {
        qCritical() << "Falha ao iniciar o servidor na porta" << port;
        return 1;
    }

    qInfo() << "BingoSys (Módulo Geral) rodando na porta" << port;

    return a.exec();
}
