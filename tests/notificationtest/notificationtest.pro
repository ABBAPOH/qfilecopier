#-------------------------------------------------
#
# Project created by QtCreator 2011-07-30T22:23:44
#
#-------------------------------------------------

QT       += core

TARGET = notificationtest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += -I $$OUT_PWD -I $$PWD/../../src/
LIBS += -L$$OUT_PWD/../../lib -lqfilecopier

SOURCES += main.cpp
