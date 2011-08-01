#include "qfilecopier_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaType>

#include <QDebug>

Q_DECLARE_METATYPE(QFileCopier::Stage)
Q_DECLARE_METATYPE(QFileCopier::Error)

QFileCopierThread::QFileCopierThread(QObject *parent) :
    QThread(parent),
    lock(QReadWriteLock::Recursive)
{
    m_stage = QFileCopier::NoStage;
    shouldEmitProgress = false;
    stopRequest = false;
    skipAllRequest = false;
}

QFileCopierThread::~QFileCopierThread()
{
    wait();
}

int QFileCopierThread::currentId()
{
    QReadLocker l(&lock);

    if (requestStack.isEmpty())
        return -1;
    return requestStack.top();
}

void QFileCopierThread::enqueueTaskList(const QList<Task> &list)
{
    QWriteLocker l(&lock);
    taskQueue.append(list);
}

QFileCopier::Stage QFileCopierThread::stage() const
{
    return m_stage;
}

void QFileCopierThread::setStage(QFileCopier::Stage stage)
{
    m_stage = stage;
    emit stageChanged(m_stage);
}

Request QFileCopierThread::request(int id) const
{
    QReadLocker l(&lock);
    return requests.at(id);
}

void QFileCopierThread::emitProgress()
{
    shouldEmitProgress = true;
}

void QFileCopierThread::cancel()
{
    QWriteLocker l(&lock);
    for (int id = 0; id < requests.size(); id++) {
        requests[id].canceled = true;
    }
}

void QFileCopierThread::cancel(int id)
{
    QWriteLocker l(&lock);
    cancelUnlocked(id);
}

void QFileCopierThread::cancelUnlocked(int id)
{
    requests[id].canceled = true;
}

void QFileCopierThread::skip()
{
    QWriteLocker l(&lock);
    if (!waitingForInteraction)
        return;

    requests[requestStack.top()].canceled = true;
    interactionCondition.wakeOne();
    waitingForInteraction = false;
}

void QFileCopierThread::skipAll()
{
    QWriteLocker l(&lock);
    if (!waitingForInteraction)
        return;

    cancelUnlocked(requestStack.top());
    skipAllRequest = true;
    interactionCondition.wakeOne();
    waitingForInteraction = false;
}

void QFileCopierThread::retry()
{
    QWriteLocker l(&lock);
    if (!waitingForInteraction)
        return;

    interactionCondition.wakeOne();
    waitingForInteraction = false;
}

void QFileCopierThread::run()
{
    bool stop = false;

    while (!stop) {

        lock.lockForWrite();
        if (!taskQueue.isEmpty()) {
            setStage(QFileCopier::Gathering);
            Task t = taskQueue.takeFirst();
            lock.unlock();

            createRequest(t);
        } else {
            lock.unlock();
        }

        if (!requestQueue.isEmpty()) {
            setStage(QFileCopier::Working);
            int id = requestQueue.takeFirst(); // inner queue, no lock
            handle(id);
        } else {
            stop = true;
        }
    }

    setStage(QFileCopier::NoStage);
}

void QFileCopierThread::createRequest(Task t)
{
    QFileInfo sourceInfo(t.source);
    QFileInfo destInfo(t.dest);
    if (destInfo.exists() && destInfo.isDir())
        t.dest = destInfo.absolutePath() + "/" + sourceInfo.fileName();

    t.dest = QDir::cleanPath(t.dest);

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
    int id = requests.size();
    requests.append(r);
    return id;
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
    int id = requests.size();
    requests.append(r);
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

bool QFileCopierThread::processRequest(const Request &r, QFileCopier::Error *err)
{
    if (r.canceled) {
        *err = QFileCopier::Canceled;
        return true;
    }

    if (!QFileInfo(r.source).exists()) {
        *err = QFileCopier::SourceNotExists;
        return false;
    }

    if (r.isDir) {

        QDir().mkpath(r.dest); // check
        foreach (int id, r.childRequests) {
            handle(id);
        }

    } else {

        QFile sourceFile(r.source);
        if (!sourceFile.open(QFile::ReadOnly)) {
            *err = QFileCopier::CannotOpenSourceFile;
            return false;
        }

        QFile destFile(r.dest);
        if (!destFile.open(QFile::WriteOnly)) {
            *err = QFileCopier::CannotOpenDestinationFile;
            return false;
        }

        const int bufferSize = 4*1024; // 4 Kb
        char *buffer = new char[bufferSize];

        qint64 totalBytesWritten = 0;
        qint64 totalFileSize = sourceFile.size();

        while (true) {
            // todo: buffersize + char buffer
            // check error while reading
            qint64 lenRead = sourceFile.read(buffer, bufferSize);
            if (lenRead == 0) {
                emit progress(totalBytesWritten, totalFileSize);
                break;
            }

            if (lenRead == -1) {
                *err = QFileCopier::CannotReadSourceFile;
                delete buffer; // scoped pointer:(
                return false;
            }

            qint64 lenWritten = 0;
            while (lenWritten < lenRead) {
                qint64 tmpLenWritten = destFile.write(buffer + lenWritten, lenRead);
                if (tmpLenWritten == -1) {
                    *err = QFileCopier::CannotWriteDestinationFile;
                    delete buffer; // scoped pointer:(
                    return false;
                }
                lenWritten += tmpLenWritten;
            }

            totalBytesWritten += lenWritten;
            if (totalFileSize < totalBytesWritten) {
                totalFileSize = totalBytesWritten;
            }

            if (shouldEmitProgress) {
                //todo : update size in request
                shouldEmitProgress = false;
                emit progress(totalBytesWritten, totalFileSize);
            }
        }
        delete [] buffer;

    }
    return true;
}

void QFileCopierThread::handle(int id)
{
    lock.lockForWrite();
    emit started(id);
    requestStack.push(id);
    lock.unlock();

    bool done = false;
    QFileCopier::Error err = QFileCopier::NoError;
    while (!done) {
        Request r = request(id);
        done = processRequest(r, &err);

        if (done || r.copyFlags & QFileCopier::NonInteractive) {
            done = true;
            if (err != QFileCopier::NoError)
                emit error(err, false);
        } else {
            lock.lockForWrite();
            if (stopRequest || skipAllError.contains(err)) {
                done = true;
                if (!stopRequest)
                    emit error(err, false);
            } else {
                emit error(err, true);
                waitingForInteraction = true;
                interactionCondition.wait(&lock);
                if (skipAllRequest) {
                    skipAllRequest = false;
                    skipAllError.insert(err);
                }
                waitingForInteraction = false;
            }
            lock.unlock();
        }
    }

    lock.lockForWrite();
    requestStack.pop();
    emit finished(id);
    lock.unlock();
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

    startThread();
}

void QFileCopierPrivate::startThread()
{
    if (!thread->isRunning()) {
        thread->start();
        setState(QFileCopier::Busy);
    }
}

void QFileCopierPrivate::onStarted(int id)
{
    emit q_func()->started(id);
}

void QFileCopierPrivate::onFinished(int id)
{
    emit q_func()->finished(id, false);
}

void QFileCopierPrivate::onThreadFinished()
{
    setState(QFileCopier::Idle);
}

void QFileCopierPrivate::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == progressTimerId) {
        thread->emitProgress();
    }
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

    qRegisterMetaType <QFileCopier::Stage> ("QFileCopier::Stage");
    qRegisterMetaType <QFileCopier::Error> ("QFileCopier::Error");

    d->thread = new QFileCopierThread(this);
    connect(d->thread, SIGNAL(stageChanged(QFileCopier::Stage)), SIGNAL(stageChanged(QFileCopier::Stage)));
    connect(d->thread, SIGNAL(started(int)), d, SLOT(onStarted(int)));
    connect(d->thread, SIGNAL(finished(int)), d, SLOT(onFinished(int)));
    connect(d->thread, SIGNAL(progress(qint64,qint64)), SIGNAL(progress(qint64,qint64)));
    connect(d->thread, SIGNAL(error(QFileCopier::Error,bool)), SIGNAL(error(QFileCopier::Error,bool)));
    connect(d->thread, SIGNAL(finished()), d, SLOT(onThreadFinished()));
    d->state = Idle;

    d->progressInterval = 500;
    d->progressTimerId = d->startTimer(d->progressInterval);
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

void QFileCopier::link(const QString &sourcePath, const QString &destinationPath, CopyFlags flags)
{
    link(QStringList() << sourcePath, destinationPath, flags);
}

void QFileCopier::link(const QStringList &sourcePaths, const QString &destinationPath, CopyFlags flags)
{
    d_func()->enqueueOperation(Task::Link, sourcePaths, destinationPath, flags);
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

QString QFileCopier::sourceFilePath(int id) const
{
    return d_func()->thread->request(id).source;
}

QString QFileCopier::destinationFilePath(int id) const
{
    return d_func()->thread->request(id).dest;
}

int QFileCopier::currentId() const
{
    return d_func()->thread->currentId();
}

QFileCopier::Stage QFileCopier::stage() const
{
    return d_func()->thread->stage();
}

QFileCopier::State QFileCopier::state() const
{
    return d_func()->state;
}

void QFileCopierPrivate::setState(QFileCopier::State s)
{
    if (state != s) {
        state = s;
        emit q_func()->stateChanged(state);
    }
}

int QFileCopier::progressInterval() const
{
    return d_func()->progressInterval;
}

void QFileCopier::setProgressInterval(int ms)
{
    Q_D(QFileCopier);

    if (d->progressInterval != ms) {
        d->killTimer(d->progressTimerId);
        d->progressInterval = ms;
        d->progressTimerId = d->startTimer(d->progressInterval);
    }
}

void QFileCopier::cancelAll()
{
    d_func()->thread->cancel();
}

void QFileCopier::cancel(int id)
{
    d_func()->thread->cancel(id);
}

void QFileCopier::skip()
{
    d_func()->thread->skip();
}

void QFileCopier::skipAll()
{
    d_func()->thread->skipAll();
}

void QFileCopier::retry()
{
    d_func()->thread->retry();
}
