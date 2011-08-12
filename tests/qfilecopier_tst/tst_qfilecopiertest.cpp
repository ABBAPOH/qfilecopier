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

class QFileCopierTest : public QObject
{
    Q_OBJECT

public:
    QFileCopierTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testCase1();

private:
    void createDirs();
    void createFiles();
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

    createDirs();
    createFiles();
}

void QFileCopierTest::cleanupTestCase()
{
    removePath(sourceFolder);
    removePath(destFolder);
}

void QFileCopierTest::testCase1()
{
    // use '/' at end of source to copy content?
    copier.copy(sourceFolder, destFolder + "/");
    copier.waitForFinished();

    QVERIFY2(exists(destFolder, dirs), "Folders were not copied");
    QVERIFY2(exists(destFolder, files), "Files were not copied");

    QVERIFY2(true, "Failure");
}

void QFileCopierTest::createDirs()
{
    QDir().mkpath(sourceFolder);
    QDir().mkpath(destFolder);
    foreach(const QString &dir, dirs) {
        QVERIFY2(QDir().mkpath(sourceFolder + QLatin1Char('/') + dir), "Cannot create dir");
    }
    QVERIFY2(exists(sourceFolder, dirs), "Dirs were not created:)");
}

void QFileCopierTest::createFiles()
{
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

bool QFileCopierTest::exists(const QString &folder, const QStringList &list)
{
    foreach (const QString &file, list) {
        if (!QFileInfo(folder + QLatin1Char('/') + file).exists()) {
            qDebug() << folder + QLatin1Char('/') + file;
            return false;
        }
    }
    return true;
}

QTEST_MAIN(QFileCopierTest)


#include "tst_qfilecopiertest.moc"
