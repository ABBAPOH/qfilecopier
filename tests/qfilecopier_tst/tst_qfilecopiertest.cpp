#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>

#include <QFileCopier>

bool removePath(const QString &path)
{
    bool result = true;
    QFileInfo info(path);
    if (info.isDir()) {
        QDir dir(path);
        foreach (const QString &entry, dir.entryList(QDir::AllEntries | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot)) {
            result &= removePath(dir.absoluteFilePath(entry));
        }
        if (!info.dir().rmdir(info.fileName()))
            return false;
    } else {
        result = QFile::remove(path);
    }
    return result;
}

class QFileCopierTest : public QObject
{
    Q_OBJECT

public:
    QFileCopierTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void testCopy1();
    void testCopy2();
    void testRemove();
    void testMove1();
    void testMove2();
    void testLink1();
    void testLink2();

private:
    void createFiles();
    bool exists(const QString &folder);
    bool exists(const QString &folder, const QStringList &list);

private:
    QString sourceFolder;
    QString destFolder;
    QStringList dirs;
    QStringList files;
    QFileCopier copier;
};

QFileCopierTest::QFileCopierTest()
{
}

void QFileCopierTest::initTestCase()
{
    sourceFolder = "source";
    destFolder = "dest";
    dirs << "folder1" << "folder2" << "folder1/folder11" << "folder1/folder11/folder111" << "folder1/folder12";
    files << "file1.bin" << "file2.bin" << "folder1/file11.bin" << "folder1/folder11/file111.bin" << "folder1/folder11/file112.bin";

    qDebug() << "remove source:" << removePath(sourceFolder);
    qDebug() << "remove dest:" << removePath(destFolder);

    createFiles();
}

void QFileCopierTest::cleanupTestCase()
{
    removePath(sourceFolder);
}

void QFileCopierTest::cleanup()
{
    removePath(destFolder);
}

void QFileCopierTest::testCopy1()
{
    copier.copy(sourceFolder, destFolder);
    copier.waitForFinished();

    QVERIFY2(exists(destFolder), "Files were not copied");
}

void QFileCopierTest::testCopy2()
{
    copier.copy(sourceFolder, destFolder + "/");
    copier.waitForFinished();

    QVERIFY2(exists(destFolder + "/" + sourceFolder), "Files were not copied");
}

void QFileCopierTest::testRemove()
{
    QString oldSourceFolder = sourceFolder;
    sourceFolder = destFolder;
    createFiles();
    sourceFolder = oldSourceFolder;

    copier.remove(destFolder);
    copier.waitForFinished();

    QVERIFY2(!exists(destFolder), "Files were not removed");
}

void QFileCopierTest::testMove1()
{
    copier.move(sourceFolder, destFolder);
    copier.waitForFinished();

    QVERIFY2(exists(destFolder) && !exists(sourceFolder), "Files were not moved");
    createFiles();
}

void QFileCopierTest::testMove2()
{
    copier.move(sourceFolder, destFolder + "/");
    copier.waitForFinished();

    QVERIFY2(exists(destFolder + "/" + sourceFolder) && !exists(sourceFolder), "Files were not moved");
    createFiles();
}

void QFileCopierTest::testLink1()
{
    copier.link(sourceFolder, destFolder);
    copier.waitForFinished();

    QFileInfo destInfo(destFolder);
    QVERIFY2(destInfo.exists() && destInfo.isSymLink() && destInfo.symLinkTarget() == QFileInfo(sourceFolder).absoluteFilePath(), "Folder were not linked");
}

void QFileCopierTest::testLink2()
{
    copier.link(sourceFolder, destFolder + "/");
    copier.waitForFinished();

    QFileInfo destInfo(destFolder + '/' + sourceFolder);
    QVERIFY2(destInfo.exists() && destInfo.isSymLink() && destInfo.symLinkTarget() == QFileInfo(sourceFolder).absoluteFilePath(), "Folder were not linked");
}

void QFileCopierTest::createFiles()
{
    QDir().mkpath(sourceFolder);
//    QDir().mkpath(destFolder);
    foreach(const QString &dir, dirs) {
        QVERIFY2(QDir().mkpath(sourceFolder + QLatin1Char('/') + dir), "Cannot create dir");
    }
    QVERIFY2(exists(sourceFolder, dirs), "Dirs were not created:)");

    foreach (const QString &file, files) {
        QFile f(sourceFolder + QLatin1Char('/') + file);
        QVERIFY2(f.open(QFile::WriteOnly), "Can't open file");
        QByteArray arr(4*1024, 0xfe);
        for (int i = 0; i < 25*1024; i++) {
            f.write(arr);
        }
    }
    QVERIFY2(exists(sourceFolder, files), "Files were not created:)");
}

bool QFileCopierTest::exists(const QString &folder)
{
    return exists(folder, dirs) && exists(folder, files);
}

bool QFileCopierTest::exists(const QString &folder, const QStringList &list)
{
    foreach (const QString &file, list) {
        if (!QFileInfo(folder + QLatin1Char('/') + file).exists()) {
//            qDebug() << folder + QLatin1Char('/') + file;
            return false;
        }
    }
    return true;
}

QTEST_MAIN(QFileCopierTest)

#include "tst_qfilecopiertest.moc"
