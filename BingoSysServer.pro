QT += core network websockets sql

QT -= gui

TARGET = BingoSysServer
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
        src/main.cpp \
        src/BingoServer.cpp \
        src/BingoTicketParser.cpp \
        src/BingoGameEngine.cpp \
        src/BingoDatabaseManager.cpp

HEADERS += \
        src/BingoServer.h \
        src/BingoTicketParser.h \
        src/BingoGameEngine.h \
        src/BingoDatabaseManager.h

# Define output directories
DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
RCC_DIR = build
UI_DIR = build
