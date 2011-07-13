#-------------------------------------------------
#
# Project created by QtCreator 2011-07-13T21:18:13
#
#-------------------------------------------------

QT       -= gui

TARGET = qfilecopier
TEMPLATE = lib

win32: DESTDIR = ./

DEFINES += QFILECOPIER_LIBRARY

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xE161A77A
    TARGET.CAPABILITY =
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = QFileCopier.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

include($$PWD/../src/src.pri)
