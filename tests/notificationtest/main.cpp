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
        foreach (const QString &entry, dir.entryList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot)) {
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
    void startTest();
    void cleanTest();

public slots:
    void onStateChanged(QFileCopier::State);
    void onStageChanged(QFileCopier::Stage);

    void onStarted(int);
    void onFinished(int);

private:
    QString tempFolder;
    QFileCopier copier;
};

void NotificationTest::initTest()
{
    tempFolder = QDesktopServices::storageLocation(QDesktopServices::TempLocation) + "/notification_test";
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
}

void NotificationTest::startTest()
{
    copier.copy(tempFolder + "/folder1", tempFolder + "/folder2");
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

    if (s == QFileCopier::NoStage) {
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
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onStateChanged", "slot invoked from wrong thread");
    qDebug() << "    Started request with id" << id
             << "(" << copier.sourceFilePath(id).mid(tempFolder.length()+1) << "->"
             << copier.destinationFilePath(id).mid(tempFolder.length()+1) << ")";
}

void NotificationTest::onFinished(int id)
{
    Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "NotificationTest::onStateChanged", "slot invoked from wrong thread");
    qDebug() << "    Finished request with id" << id;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    NotificationTest t;
    t.initTest();
    t.startTest();
//    t.cleanTest();

//    QTimer::singleShot(5000, &a, SLOT(quit()));

    return a.exec();
}

#include "main.moc"
