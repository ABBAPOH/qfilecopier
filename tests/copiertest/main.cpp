#include <QtCore/QCoreApplication>

#include "qfilecopier.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QFileCopier copier;

//    copier.copy("/Users/arch/cards.xml", "/Users/arch/Desktop/cards2.xml");
    copier.copy("/Users/arch/folder", "/Users/arch/Desktop/foooolder");

    return a.exec();
}
