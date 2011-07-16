#include "qfilecopier_p.h"

QFileCopierThread::QFileCopierThread(QObject *parent) :
    QThread(parent)
{
}

void QFileCopierThread::copy(const QStringList &sourcePaths, const QString &destinationPath,
                             QFileCopier::CopyFlags flags)
{
    QMutexLocker l(&mutex);
    foreach (const QString &path, sourcePaths) {
        Request r;
        r.source = path;
        r.dest = destinationPath;
        r.copyFlags = flags;
        infoQueue.append(r);
    }
}

void QFileCopierThread::run()
{
    bool stop = false;

    while (!stop) {
        if (!infoQueue.isEmpty()) {
            // setState(gathering)
            mutex.lock();
            ::Request r = infoQueue.takeFirst();
            mutex.unlock();
            updateRequest(r);
        }
    }
}

void QFileCopierThread::updateRequest(::Request r)
{
    Request result;
    result.source = r.source;
    result.copyFlags = r.copyFlags;

    QFileInfo sourceInfo(r.source);
    QFileInfo destInfo(r.dest);
    if (destInfo.exists() && destInfo.isDir())
        r.dest = r.dest + QDir::separator() + sourceInfo.fileName();

    if (sourceInfo.isDir())
        addDirToQueue(r);
    else
        addFileToQueue(r);
}

int QFileCopierThread::addFileToQueue(const ::Request & request)
{
    Request r;
    r.type = request.type;
    r.source = request.source;
    r.dest = request.dest;
    r.copyFlags = request.copyFlags;
    r.isDir = false;
    requestQueue.append(r);
    return requestQueue.size();
}

int QFileCopierThread::addDirToQueue(const::Request &request)
{
    Request r;
    r.type = request.type;
    r.source = request.source;
    r.dest = request.dest;
    r.copyFlags = request.copyFlags;
    r.isDir = true;
    requestQueue.append(r);

    QDirIterator i(r.source, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        QString source = i.next();
        QFileInfo sourceInfo(source);
        ::Request request;
        request.type = r.type;
        request.source = source;
        request.dest = r.dest + QDir::separator() + sourceInfo.fileName();
        request.copyFlags = r.copyFlags;
        if (sourceInfo.isDir())
            r.childRequests.append(addDirToQueue(request));
        else
            r.childRequests.append(addFileToQueue(request));
    }
    return requestQueue.size();
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
    d->thread->start();
}

QFileCopier::~QFileCopier()
{
    delete d_ptr;
}

void QFileCopier::copy(const QString &sourcePath, const QString &destinationPath, CopyFlags flags)
{
    d_func()->thread->copy(QStringList() << sourcePath, destinationPath, flags);
}
