#include <QtCore/QCoreApplication>

#include "qfilecopier.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QFileCopier copier;

    return a.exec();
}
