QT += core network websockets

QT -= gui

TARGET = BingoSysServer
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
        src/main.cpp \
        src/BingoServer.cpp \
        src/BingoTicketParser.cpp \
        src/BingoGameEngine.cpp

HEADERS += \
        src/BingoServer.h \
        src/BingoTicketParser.h \
        src/BingoGameEngine.h

# Define output directories
DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
RCC_DIR = build
UI_DIR = build
