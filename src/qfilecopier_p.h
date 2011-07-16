#ifndef QFILECOPIER_P_H
#define QFILECOPIER_P_H

#include "qfilecopier.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QThread>

struct Request
{
    enum Type { Copy, Move, Link };

    Type type;
    QString source;
    QString dest;
    QFileCopier::CopyFlags copyFlags;
};

class QFileCopierThread : public QThread
{
    Q_OBJECT

public:
    struct Request : public ::Request
    {
        bool isDir;
        QList<int> childRequests;
    };

    explicit QFileCopierThread(QObject *parent = 0);

    void copy(const QStringList &sourcePaths, const QString &destinationPath, QFileCopier::CopyFlags flags);

protected:
    void run();

signals:

private:
    void updateRequest(::Request r);
    int addFileToQueue(const ::Request &request);
    int addDirToQueue(const ::Request &request);

private:
    mutable QMutex mutex;
    QQueue< ::Request > infoQueue;
    QList<Request> requestQueue;
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
