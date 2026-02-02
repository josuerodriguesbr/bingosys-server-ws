#include "BingoTicketParser.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

BingoTicketParser::BingoTicketParser()
{
}

BingoTicket BingoTicketParser::parseLine(const QString &line)
{
    BingoTicket ticket;
    ticket.id = -1;
    ticket.checkDigit = -1;

    QStringList parts = line.split('-');
    if (parts.isEmpty()) {
        return ticket;
    }

    // Part 0: ID + CheckDigit
    // Example: 0000019 -> ID 000001, Digit 9
    QString idPart = parts.first();
    if (idPart.length() > 1) {
        // Last char is check digit
        QString digitStr = idPart.right(1);
        QString idStr = idPart.left(idPart.length() - 1);
        
        ticket.id = idStr.toInt();
        ticket.checkDigit = digitStr.toInt();
    }

    // Process other parts (Grids)
    for (int i = 1; i < parts.size(); ++i) {
        QString gridStr = parts[i];
        if (!gridStr.isEmpty()) {
            ticket.grids.append(parseGridString(gridStr));
        }
    }

    return ticket;
}

QVector<int> BingoTicketParser::parseGridString(const QString &gridStr)
{
    QVector<int> numbers;
    // Each number has 2 digits.
    // 011012... -> 01, 10, 12...
    for (int i = 0; i < gridStr.length(); i += 2) {
        if (i + 1 < gridStr.length()) {
            QString numStr = gridStr.mid(i, 2);
            numbers.append(numStr.toInt());
        }
    }
    return numbers;
}

QVector<BingoTicket> BingoTicketParser::parseFile(const QString &filePath)
{
    QVector<BingoTicket> tickets;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open file:" << filePath;
        return tickets;
    }

    // Tenta reservar espaço para evitar realocações lentas
    tickets.reserve(600000); 

    QTextStream in(&file);
    int count = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.trimmed().isEmpty()) {
            tickets.append(parseLine(line));
            count++;
            if (count % 100000 == 0) {
                qInfo() << "Lendo cartelas..." << count;
            }
        }
    }
    
    file.close();
    return tickets;
}
