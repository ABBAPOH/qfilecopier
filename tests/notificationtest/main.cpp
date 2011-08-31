#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore>

#include <QtGui/QDesktopServices>

#include <QFileCopier>

bool removePath(const QString &path)
{
    bool result = true;
    QFileInfo info(path);
    if (info.isDir()) {
        QDir dir(path);
        foreach (const QString &entry, dir.entryList(QDir::AllDirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)) {
            result &= removePath(dir.absoluteFilePath(entry));
        }
        if (!info.dir().rmdir(info.fileName()))
            return false;
    } else {
        result = QFile::remove(path);
    }
    return result;
}

class NotificationTest : public QObject
{
    Q_OBJECT

public:
    void initTest();

    void testCopy();
    void testMove();
    void testRename();
    void testLink();
    void testRemove();

    void cleanTest();

public slots:
    void onStateChanged(QFileCopier::State);
    void onStageChanged(QFileCopier::Stage);

    void onStarted(int);
    void onFinished(int);
    void onProgress(qint64, qint64);

    void onError(int id, QFileCopier::Error error, bool stopped);

private:
    QString tempFolder;
    QFileCopier copier;
};

void NotificationTest::initTest()
{
    tempFolder = QDesktopServices::storageLocation(QDesktopServices::TempLocation) + "/notification_test";
    removePath(tempFolder);
    qDebug() << "using temp folder" << tempFolder;
    QDir().mkpath(tempFolder);
    QDir d(tempFolder);
    d.mkdir("folder1");
    QFile f(tempFolder + "/folder1/" + "file.bin");
    f.open(QFile::WriteOnly);
    QByteArray arr(1024, 0xfe);
    for (int i = 0; i < 100*1024; i++) {
        f.write(arr);
    }
    f.close();

    connect(&copier, SIGNAL(stateChanged(QFileCopier::State)), SLOT(onStateChanged(QFileCopier::State)));
    connect(&copier, SIGNAL(stageChanged(QFileCopier::Stage)), SLOT(onStageChanged(QFileCopier::Stage)));

    connect(&copier, SIGNAL(started(int)), SLOT(onStarted(int)));
    connect(&copier, SIGNAL(finished(int,bool)), SLOT(onFinished(int)));
    connect(&copier, SIGNAL(progress(qint64,qint64)), SLOT(onProgress(qint64,qint64)));

    connect(&copier, SIGNAL(error(int, QFileCopier::Error,bool)), SLOT(onError(int, QFileCopier::Error,bool)));
}

void NotificationTest::testCopy()
{
    qDebug() << "testCopy";
//    QDir().mkpath(tempFolder + "/folder2/folder1");
    copier.copy(tempFolder + "/folder1", tempFolder + "/folder2", QFileCopier::Force);
}

void NotificationTest::testMove()
{
    qDebug() << "testMove";
    copier.move(tempFolder + "/folder1", tempFolder + "/folder2", QFileCopier::CopyOnMove);
}

void NotificationTest::testRename()
{
    qDebug() << "testRename";
    copier.move(tempFolder + "/folder1", tempFolder + "/folder2");
}

void NotificationTest::testLink()
{
    qDebug() << "testLink";
    copier.link(tempFolder + "/folder1", tempFolder + "/folder2");
}

void NotificationTest::testRemove()
{
    qDebug() << "testRemove";
    copier.remove(tempFolder + "/folder1");
}

void NotificationTest::cleanTest()
{
    qDebug() << "";
    qDebug() << "Cleaning test";
    qDebug() << "  Removing tmp folder" << removePath(tempFolder);
}

void NotificationTest::onStateChanged(QFileCopier::State s)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onStateChanged", "slot invoked from wrong thread");
    qDebug() << "State changed" << s;

    if (s == QFileCopier::Idle) {
        cleanTest();
        qApp->quit();
    }
}

void NotificationTest::onStageChanged(QFileCopier::Stage s)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onStateChanged", "slot invoked from wrong thread");
    qDebug() << "  Stage changed" << s;
}

void NotificationTest::onStarted(int id)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onStarted", "slot invoked from wrong thread");
    qDebug() << "    Started request with id" << id
             << "(" << copier.sourceFilePath(id).mid(tempFolder.length()+1) << "->"
             << copier.destinationFilePath(id).mid(tempFolder.length()+1) << ")";
}

void NotificationTest::onFinished(int id)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onFinished", "slot invoked from wrong thread");
    qDebug() << "    Finished request with id" << id;
}

void NotificationTest::onProgress(qint64 w, qint64 s)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onProgress", "slot invoked from wrong thread");
    qDebug() << QString("      Progress %1 / %2 (%3)").arg(w/1024).arg(s/1024).arg(s == 0 ? 0 : 100*w/s).toLatin1().data();
    qDebug() << QString("      Total progress %1 / %2 (%3)").arg(copier.totalProgress()/1024).arg(copier.totalSize()/1024).arg(100*copier.totalProgress()/copier.totalSize()).toLatin1().data();
}

void NotificationTest::onError(int id, QFileCopier::Error error, bool stopped)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onProgress", "slot invoked from wrong thread");
    qDebug() << "      Error occured for request" << copier.currentId() << error << "stopped ="<< stopped;

    if (error == QFileCopier::DestinationExists)
        copier.overwrite();
    else
        copier.skip();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    NotificationTest t;
    t.initTest();

    t.testCopy();
//    t.testMove();
//    t.testRename();
//    t.testLink();
//    t.testRename();

    return a.exec();
}

#include "main.moc"
