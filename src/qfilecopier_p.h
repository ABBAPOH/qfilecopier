#ifndef QFILECOPIER_P_H
#define QFILECOPIER_P_H

#include "qfilecopier.h"

#include <QtCore/QThread>

class QFileCopierThread : public QThread
{
    Q_OBJECT

public:
    explicit QFileCopierThread(QObject *parent = 0);

protected:
    void run();

signals:

};

class QFileCopierPrivate
{
    Q_DECLARE_PUBLIC(QFileCopier)

public:
    QFileCopierPrivate(QFileCopier *qq) : q_ptr(qq) {}

    QFileCopierThread *thread;

private:
    QFileCopier *q_ptr;
};

#endif // QFILECOPIER_P_H
