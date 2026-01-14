#ifndef BINGOTICKETPARSER_H
#define BINGOTICKETPARSER_H

#include <QString>
#include <QList>
#include <QVector>

struct BingoTicket {
    int id;
    int checkDigit;
    // Map or List of grids? List is safer since index matters.
    // Index 0 = Part 1 (after ID), Index 1 = Part 2, etc.
    QVector<QVector<int>> grids;
};

class BingoTicketParser
{
public:
    BingoTicketParser();
    
    // Parses a single line string into a BingoTicket struct
    static BingoTicket parseLine(const QString &line);
    
    // Reads all lines from a file
    static QVector<BingoTicket> parseFile(const QString &filePath);

private:
    static QVector<int> parseGridString(const QString &gridStr);
};

#endif // BINGOTICKETPARSER_H
