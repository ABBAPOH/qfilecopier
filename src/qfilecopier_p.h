#ifndef QFILECOPIER_P_H
#define QFILECOPIER_P_H

#include "qfilecopier.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtCore/QThread>
#include <QtCore/QWaitCondition>

struct Task
{
    enum Type { Copy, Move, Remove, Link };

    Type type;
    QString source;
    QString dest;
    QFileCopier::CopyFlags copyFlags;
};

struct Request : public Task
{
    Request() : isDir(false), canceled(false), overwrite(false), merge(false) {}

    bool isDir;
    QList<int> childRequests;
    qint64 size;

    bool canceled;
    bool overwrite;
    bool merge;
};

class QFileCopierThread : public QThread
{
    Q_OBJECT

    typedef QFileCopier::Stage Stage;
    Q_ENUMS(QFileCopier::Stage)
public:
    explicit QFileCopierThread(QObject *parent = 0);
    ~QFileCopierThread();

    int currentId();

    void enqueueTaskList(const QList<Task> &list);

    QFileCopier::Stage stage() const;
    void setStage(QFileCopier::Stage);

    Request request(int id) const;

    qint64 totalProgress() const;
    qint64 totalSize() const;

    void emitProgress();

    void cancel();
    void cancel(int id);
    void skip();
    void skipAll();
    void retry();

    void overwrite();
    void overwriteAll();

    void resetSkip();
    void resetOverwrite();

    void merge();
    void mergeAll();

protected:
    void run();

signals:
    void stageChanged(QFileCopier::Stage);
    void started(int);
    void finished(int);
    void progress(qint64 progress, qint64 size);
    void error(QFileCopier::Error error, bool stopped);
    void done(bool error);
    void canceled();

private:
    void createRequest(Task r);
    bool shouldOverwrite(const Request &r);
    bool shouldMerge(const Request &r);
    bool checkRequest(int id);
    int addFileToQueue(const Task &r);
    int addDirToQueue(const Task &r);
    bool interact(const Request &r, bool done, QFileCopier::Error err);
    bool createDir(const Request &r, QFileCopier::Error *err);
    bool copyFile(const Request &r, QFileCopier::Error *err);
    bool copy(const Request &, QFileCopier::Error *);
    bool move(const Request &, QFileCopier::Error *);
    bool link(const Request &, QFileCopier::Error *);
    bool remove(const Request &, QFileCopier::Error *);
    bool processRequest(const Request &, QFileCopier::Error *);
    void handle(int id);
    void overwriteChildren(int id);

private:
    mutable QReadWriteLock lock;
    QQueue<Task> taskQueue;
    QQueue<int> requestQueue;
    QList<Request> requests;
    QStack<int> requestStack;

    volatile QFileCopier::Stage m_stage;
    volatile bool shouldEmitProgress;
    bool stopRequest;
    QWaitCondition interactionCondition;
    bool waitingForInteraction;
    bool skipAllRequest;
    QSet<QFileCopier::Error> skipAllError;
    bool cancelAllRequest;
    bool hasError;
    bool overwriteAllRequest;
    bool mergeAllRequest;
    qint64 m_totalProgress;
    qint64 m_totalSize;
};

class QFileCopierPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QFileCopier)

public:
    QFileCopierPrivate(QFileCopier *qq) : q_ptr(qq) {}

    QFileCopierThread *thread;
    QFileCopier::State state;
    int progressTimerId;
    int progressInterval;

    void enqueueOperation(Task::Type operationType, const QStringList &sourcePaths,
                          const QString &destinationPath, QFileCopier::CopyFlags flags);
    void startThread();

    void setState(QFileCopier::State s);

public slots:
    void onStarted(int);
    void onFinished(int);
    void onThreadFinished();

protected:
    void timerEvent(QTimerEvent *);

private:
    QFileCopier *q_ptr;
};


#endif // QFILECOPIER_P_H
