#-------------------------------------------------
#
# Project created by QtCreator 2011-07-13T21:25:38
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = copiertest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += ../../src
LIBS += -L$$OUT_PWD/../../lib -lqfilecopier

SOURCES += main.cpp
