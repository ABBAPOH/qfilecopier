#ifndef QFILECOPIER_P_H
#define QFILECOPIER_P_H

#include "qfilecopier.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QQueue>
#include <QtCore/QStack>
#include <QtCore/QThread>

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
    bool isDir;
    QList<int> childRequests;
};

class QFileCopierThread : public QThread
{
    Q_OBJECT

    typedef QFileCopier::Stage Stage;
    Q_ENUMS(QFileCopier::Stage)
public:
    explicit QFileCopierThread(QObject *parent = 0);
    ~QFileCopierThread();

    void enqueueTaskList(const QList<Task> &list);

    QFileCopier::Stage stage() const;
    void setStage(QFileCopier::Stage);

    Request request(int id) const;

protected:
    void run();

signals:
    void stageChanged(QFileCopier::Stage);
    void started(int);
    void finished(int);

private:
    void createRequest(Task r);
    int addFileToQueue(const Task &r);
    int addDirToQueue(const Task &r);
    void processRequest(int id);

private:
    mutable QReadWriteLock lock;
    QQueue<Task> taskQueue;
    QQueue<int> requestQueue;
    QList<Request> requests;

    volatile QFileCopier::Stage m_stage;
};

class QFileCopierPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QFileCopier)

public:
    QFileCopierPrivate(QFileCopier *qq) : q_ptr(qq) {}

    QFileCopierThread *thread;
    QFileCopier::State state;

    void enqueueOperation(Task::Type operationType, const QStringList &sourcePaths,
                          const QString &destinationPath, QFileCopier::CopyFlags flags);
    void startThread();

    void setState(QFileCopier::State s);

public slots:
    void onStarted(int);
    void onFinished(int);
    void onThreadFinished();

private:
    QStack<int> currentRequests;

    QFileCopier *q_ptr;
};


#endif // QFILECOPIER_P_H
