#include "Library.h"
#include "PlayerController.h"
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>
#include <clocale>

class PlayerTests : public QObject {
    Q_OBJECT
    static QUrl fixture(const QString &name)
    {
        return QUrl::fromLocalFile(QString(FIXTURE_DIR) + "/" + name + ".m4b");
    }
private slots:
    void nonCNumericLocaleStartup()
    {
        const QByteArray previous = std::setlocale(LC_NUMERIC, nullptr);
        const bool available = std::setlocale(LC_NUMERIC, "fi_FI.UTF-8") != nullptr;
        const bool commaDecimal = available && QByteArray(std::localeconv()->decimal_point) == ",";
        std::setlocale(LC_NUMERIC, previous.constData());
        if (!commaDecimal)
            QSKIP("Finnish comma-decimal locale is not installed");

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("LC_ALL", "fi_FI.UTF-8");
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("QT_QUICK_BACKEND", "software");
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(QCoreApplication::applicationDirPath() + "/qt-m4b-player",
            {"--smoke-test", "--data-dir", directory.path(), fixture("captions").toLocalFile()});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(10000));
        const QByteArray errors = process.readAllStandardError();
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, errors.constData());
        QVERIFY2(!errors.contains("Binding loop"), errors.constData());
    }
    void realPausedSeekingAndTransport()
    {
        QTemporaryDir directory;
        Library library(directory.filePath("library.sqlite"));
        PlayerController player(&library, directory.filePath("covers"), true);
        player.openBook(fixture("captions"));
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && player.chapters().size() == 2 && player.captionsAvailable(), 10000);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
        QVERIFY(!player.playing());
        player.setCaptions(true);
        player.seek(1);
        QTRY_COMPARE(player.captionText(), QString::fromUtf8("First caption — café."));
        QTest::qWait(100);
        QVERIFY(!player.playing());
        QVERIFY(std::abs(player.position() - 1.0) < 0.05);
        player.seek(2);
        QVERIFY(player.captionText().isEmpty());
        player.seek(4);
        QCOMPARE(player.currentChapter(), 1);
        QCOMPARE(player.captionText(), "Late caption with two lines.\nSecond line.");
        player.navigateChapter(-1);
        QCOMPARE(player.currentChapter(), 0);
        QVERIFY(!player.playing());
        player.navigateChapter(1);
        QCOMPARE(player.currentChapter(), 1);
        QVERIFY(std::abs(player.position() - 4.0) < 0.05);
        player.stop();
        QCOMPARE(player.currentChapter(), 1);
        QVERIFY(std::abs(player.position() - 3.0) < 0.05);
        player.setSpeed(1.5);
        QTRY_COMPARE(player.speed(), 1.5);
        player.togglePlayback();
        QTRY_VERIFY(player.playing());
        QTRY_VERIFY(player.position() > 3.2);
        player.togglePlayback();
        QTRY_VERIFY(!player.playing());
        player.setStopAfterChapter(true);
        player.seek(2.8);
        player.togglePlayback();
        QTRY_VERIFY(!player.playing() && std::abs(player.position() - 3) < 0.05);
        QCOMPARE(player.currentChapter(), 1);
    }
    void savedResumeAndRapidReplacement()
    {
        QTemporaryDir directory;
        const QString db = directory.filePath("library.sqlite");
        {
            Library library(db);
            PlayerController player(&library, directory.filePath("covers"), true);
            player.openBook(fixture("captions"));
            QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && player.captionsAvailable(), 10000);
            player.seek(4);
            player.setCaptions(true);
            player.shutdown();
        }
        Library library(db);
        PlayerController player(&library, directory.filePath("covers"), true);
        player.openBook(fixture("captions"));
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && player.captionsAvailable(), 10000);
        QTRY_VERIFY(std::abs(player.position() - 4) < 0.05);
        QVERIFY(player.captionsEnabled());
        QVERIFY(!player.playing());
        player.openBook(fixture("long-captions"));
        player.openBook(fixture("no-captions"));
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && !player.indexingCaptions() && player.chapters().size() == 2, 10000);
        QCOMPARE(player.fileUrl(), fixture("no-captions"));
        QVERIFY(!player.captionsAvailable());
        QVERIFY(player.captionText().isEmpty());
        QVERIFY(!player.playing());
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
    }
    void roundedChapterStartAndStopAfter()
    {
        QTemporaryDir directory;
        Library library(directory.filePath("library.sqlite"));
        PlayerController player(&library, directory.filePath("covers"), true);
        player.openBook(fixture("fractional-chapters"));
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && player.chapters().size() == 2, 10000);
        const double start = player.chapters()[1].toMap().value("start").toDouble();
        QCOMPARE(start, 3.621);
        player.setStopAfterChapter(true);

        // AAC's paused clock can settle slightly before the requested start.
        player.selectChapter(1);
        QTest::qWait(1200);
        QCOMPARE(player.currentChapter(), 1);
        QCOMPARE(player.position(), start);
        player.togglePlayback();
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > start + 0.3, 3000);
        QVERIFY(player.playing());

        // Selections during playback must replace the previous chapter's cutoff.
        player.selectChapter(0);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.3 && player.position() < 1, 3000);
        QCOMPARE(player.currentChapter(), 0);
        QVERIFY(player.playing());
        player.selectChapter(1);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > start + 0.3, 3000);
        QCOMPARE(player.currentChapter(), 1);
        QVERIFY(player.playing());
        player.togglePlayback();
        QTRY_VERIFY(!player.playing());

        // A chapter-end stop also leaves a usable next-chapter start.
        player.seek(start - 0.2);
        player.togglePlayback();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playing() && player.position() == start, 3000);
        QTest::qWait(1200);
        QCOMPARE(player.currentChapter(), 1);
        QCOMPARE(player.position(), start);
        player.togglePlayback();
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > start + 0.3, 3000);
        QVERIFY(player.playing());
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
    }
    void optionalExternalBook()
    {
        const QString path = qEnvironmentVariable("QT_M4B_TEST_BOOK");
        if (path.isEmpty())
            QSKIP("Set QT_M4B_TEST_BOOK to validate an additional local/NAS book");
        QTemporaryDir directory;
        Library library(directory.filePath("library.sqlite"));
        PlayerController player(&library, directory.filePath("covers"), true);
        int heartbeats = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&heartbeats] { ++heartbeats; });
        heartbeat.start(20);
        QElapsedTimer submitted;
        submitted.start();
        player.openLocalPath(path);
        QVERIFY2(submitted.elapsed() < 500, "Opening a network book blocked the caller");
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded() && !player.chapters().isEmpty(), 15000);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
        const double target = std::min(1000.0, player.duration() / 2);
        submitted.restart();
        player.seek(target);
        QVERIFY2(submitted.elapsed() < 500, "Seeking a network book blocked the caller");
        QTRY_VERIFY_WITH_TIMEOUT(!player.indexingCaptions(), 45000);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
        QVERIFY(heartbeats > 0);
        QVERIFY(!player.playing());
        QVERIFY(std::abs(player.position() - target) < 0.1);
        int selected = player.chapters().size() / 2;
        for (int i = 0; i < player.chapters().size(); ++i) {
            if (player.chapters()[i].toMap().value("title").toString() == "Chapter Twelve")
                selected = i;
        }
        const double start = player.chapters()[selected].toMap().value("start").toDouble();
        player.setStopAfterChapter(true);
        player.selectChapter(selected);
        QTest::qWait(1500);
        QCOMPARE(player.currentChapter(), selected);
        QCOMPARE(player.position(), start);
        player.togglePlayback();
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > start + 0.3, 3000);
        QVERIFY(player.playing());
        player.togglePlayback();
        // Read-only compatibility check; all progress/artwork use the temporary library.
    }
};
QTEST_GUILESS_MAIN(PlayerTests)
#include "PlayerTests.moc"
