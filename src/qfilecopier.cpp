#include "qfilecopier_p.h"

#include <QCoreApplication>
#include <QDebug>

QFileCopierThread::QFileCopierThread(QObject *parent) :
    QThread(parent),
    currentRequest(0)
{
}

void QFileCopierThread::copy(const QStringList &sourcePaths, const QString &destinationPath,
                             QFileCopier::CopyFlags flags)
{
    QList< Task > requests;
    foreach (const QString &path, sourcePaths) {
        Task r;
        r.source = path;
        r.dest = destinationPath;
        r.copyFlags = flags;
        requests.append(r);
    }
    QMutexLocker l(&mutex);
    infoQueue.append(requests);
}

void QFileCopierThread::run()
{
    bool stop = false;

    while (!stop) {
        if (!infoQueue.isEmpty()) {
            // setState(gathering)
            mutex.lock();
            Task r = infoQueue.takeFirst();
            mutex.unlock();
            updateRequest(r);
            // todo: use second queue
        } else if (currentRequest < requestQueue.size()) {
            processRequest(currentRequest);
            currentRequest++;
        } else {
            stop = true;
        }
    }
}

void QFileCopierThread::updateRequest(Task r)
{
    QFileInfo sourceInfo(r.source);
    QFileInfo destInfo(r.dest);
    if (destInfo.exists() && destInfo.isDir())
        r.dest = r.dest + QDir::separator() + sourceInfo.fileName();
    // todo: fix / at end of non-existing path

    int index = -1;
    if (sourceInfo.isDir())
        index = addDirToQueue(r);
    else
        index = addFileToQueue(r);
//    copyQueue.append(index);
}

int QFileCopierThread::addFileToQueue(const Task & request)
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

int QFileCopierThread::addDirToQueue(const Task &request)
{
    Request r;
    r.type = request.type;
    r.source = request.source;
    r.dest = request.dest;
    r.copyFlags = request.copyFlags;
    r.isDir = true;
    requestQueue.append(r);
    int id = requestQueue.size();

    QDirIterator i(r.source, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        QString source = i.next();
        QFileInfo sourceInfo(source);
        Task request;
        request.type = r.type;
        request.source = source;
        request.dest = r.dest + QDir::separator() + sourceInfo.fileName();
        request.copyFlags = r.copyFlags;
        if (sourceInfo.isDir())
            requestQueue[id].childRequests.append(addDirToQueue(request));
        else
            requestQueue[id].childRequests.append(addFileToQueue(request));
    }
    return requestQueue.size();
}

void QFileCopierThread::processRequest(int id)
{
    emit started(id);
    const Request &r = requestQueue.at(id);

    currentRequest = id;
    if (r.isDir) {
        QDir().mkpath(r.dest); // check
        foreach (int id, r.childRequests) {
            processRequest(id);
        }
    } else {
        QFile s(r.source);
        s.open(QFile::ReadOnly); // check if opened
        QFile d(r.dest);
        d.open(QFile::WriteOnly);
        while (!s.atEnd()) {
            // todo: buffersize + char buffer
            // check error while reading
            QByteArray chunk = s.read(1024);
            d.write(chunk);
//            if (shouldEmit)
//                emit progress(currentWritten);
        }
    }
    emit finished(id);
}

void QFileCopierPrivate::onStarted(int id)
{
    currentRequests.append(id);
    qDebug() << "started id" << id << QThread::currentThread() << qApp->thread();
}

void QFileCopierPrivate::onFinished(int id)
{
    qDebug() << "finished id" << id << QThread::currentThread() << qApp->thread();
    currentRequests.pop();
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
    connect(d->thread, SIGNAL(started(int)), d, SLOT(onStarted(int)));
    connect(d->thread, SIGNAL(finished(int)), d, SLOT(onFinished(int)));
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
