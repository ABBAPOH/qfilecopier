#include "qfilecopier_p.h"

#include <QCoreApplication>
#include <QDebug>

QFileCopierThread::QFileCopierThread(QObject *parent) :
    QThread(parent),
    lock(QReadWriteLock::Recursive)
{
}

void QFileCopierThread::enqueueTaskList(const QList<Task> &list)
{
    QWriteLocker l(&lock);
    taskQueue.append(list);
}

void QFileCopierThread::run()
{
    bool stop = false;

    while (!stop) {
        lock.lockForWrite();
        if (!taskQueue.isEmpty()) {
            // setState(gathering)
            Task r = taskQueue.takeFirst();
            lock.unlock();

            updateRequest(r);
            // todo: use second queue
        } else {
            lock.unlock();
        }
        if (requestQueue.isEmpty()) {
            int id = requestQueue.takeFirst(); // inner queue, no lock
            processRequest(id);
        } else {
            stop = true;
        }
    }
}

void QFileCopierThread::updateRequest(Task t)
{
    QFileInfo sourceInfo(t.source);
    QFileInfo destInfo(t.dest);
    if (destInfo.exists() && destInfo.isDir())
        t.dest = QDir::fromNativeSeparators(t.dest) + "/" + sourceInfo.fileName();
    else
        t.dest = QDir::fromNativeSeparators(t.dest);
    // todo: fix / at end of non-existing path

    int index = -1;
    if (sourceInfo.isDir())
        index = addDirToQueue(t);
    else
        index = addFileToQueue(t);
    requestQueue.append(index);
}

int QFileCopierThread::addFileToQueue(const Task & task)
{
    Request r;
    r.type = task.type;
    r.source = task.source;
    r.dest = task.dest;
    r.copyFlags = task.copyFlags;
    r.isDir = false;

    QWriteLocker l(&lock);
    requests.append(r);
    return requests.size();
}

int QFileCopierThread::addDirToQueue(const Task &task)
{
    Request r;
    r.type = task.type;
    r.source = task.source;
    r.dest = task.dest;
    r.copyFlags = task.copyFlags;
    r.isDir = true;

    lock.lockForWrite();
    requests.append(r);
    int id = requests.size();
    lock.unlock();

    QList<int> childRequests;

    QDirIterator i(r.source, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        QString source = i.next();
        QFileInfo sourceInfo(source);

        Task t;
        t.type = r.type;
        t.source = source;
        t.dest = r.dest + "/" + sourceInfo.fileName();
        t.copyFlags = r.copyFlags;

        if (sourceInfo.isDir())
            childRequests.append(addDirToQueue(t));
        else
            childRequests.append(addFileToQueue(t));
    }

//    QWriteLocker l(&lock); // i think no need as QList hac atomic int within
    requests[id].childRequests = childRequests;

    return id;
}

void QFileCopierThread::processRequest(int id)
{
    emit started(id);

    const Request &r = requests.at(id);

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


void QFileCopierPrivate::enqueueOperation(Task::Type operationType, const QStringList &sourcePaths,
                                          const QString &destinationPath, QFileCopier::CopyFlags flags)
{
    QList<Task> taskList;
    taskList.reserve(sourcePaths.size());
    foreach (const QString &path, sourcePaths) {
        Task t;
        t.source = path;
        t.dest = destinationPath;
        t.copyFlags = flags;
        t.type = operationType;
        taskList.append(t);
    }
    thread->enqueueTaskList(taskList);
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
    copy(QStringList() << sourcePath, destinationPath, flags);
}

void QFileCopier::copy(const QStringList &sourcePaths, const QString &destinationPath, CopyFlags flags)
{
    d_func()->enqueueOperation(Task::Copy, sourcePaths, destinationPath, flags);
}

void QFileCopier::move(const QString &sourcePath, const QString &destinationPath, CopyFlags flags)
{
    move(QStringList() << sourcePath, destinationPath, flags);
}

void QFileCopier::move(const QStringList &sourcePaths, const QString &destinationPath, CopyFlags flags)
{
    d_func()->enqueueOperation(Task::Move, sourcePaths, destinationPath, flags);
}

void QFileCopier::remove(const QString &path, CopyFlags flags)
{
    remove(QStringList() << path, flags);
}

void QFileCopier::remove(const QStringList &paths, CopyFlags flags)
{
    d_func()->enqueueOperation(Task::Remove, paths, QString(), flags);
}

