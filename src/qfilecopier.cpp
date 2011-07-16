#include "qfilecopier_p.h"

QFileCopierThread::QFileCopierThread(QObject *parent) :
    QThread(parent)
{
}

void QFileCopierThread::run()
{
}

/*!
    \class QFileCopier

    \brief The QFileCopier class allows you to copy and move files in a background process.
*/
QFileCopier::QFileCopier(QObject *parent) :
    QObject(parent),
    d_ptr(new QFileCopierPrivate(this))
{
    Q_D(QFileCopier);

    d->thread = new QFileCopierThread(this);
}

QFileCopier::~QFileCopier()
{
    delete d_ptr;
}
