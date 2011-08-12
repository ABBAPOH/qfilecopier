#-------------------------------------------------
#
# Project created by QtCreator 2011-08-13T02:13:11
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = tst_qfilecopiertest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += -I $$OUT_PWD -I $$PWD/../../src/
LIBS += -L$$OUT_PWD/../../lib -lqfilecopier

SOURCES += tst_qfilecopiertest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
