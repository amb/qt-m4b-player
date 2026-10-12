#include "Library.h"
#include "MediaData.h"
#include "MetadataReader.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void embeddedTrackAndChapterTiming()
    {
        const auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto book = MetadataReader::read(QString(FIXTURE_DIR) + "/captions.m4b", cancel, true);
        QVERIFY2(book.error.isEmpty(), qPrintable(book.error));
        QCOMPARE(book.chapters.size(), 2);
        QCOMPARE(book.chapters[0].title, "First chapter");
        QCOMPARE(book.chapters[0].start, 0.0);
        QCOMPARE(book.chapters[0].end, 3.0);
        QCOMPARE(book.chapters[1].start, 3.0);
        QCOMPARE(book.duration, 6.0);
        QCOMPARE(book.captions.size(), 2);
        QCOMPARE(book.captions[0].start, 0.5);
        QCOMPARE(book.captions[0].end, 1.5);
        QCOMPARE(book.captions[0].text, QString::fromUtf8("First caption — café."));
        QCOMPARE(book.captions[1].start, 3.0);
        QCOMPARE(book.captions[1].end, 5.5);
        QCOMPARE(book.captions[1].text, "Late caption with two lines.\nSecond line.");
        QVERIFY(!book.cover.isEmpty());
        QVERIFY(captionAt(book.captions, 0) == nullptr);
        QCOMPARE(captionAt(book.captions, 0.5)->text, book.captions[0].text);
        QVERIFY(captionAt(book.captions, 1.5) == nullptr);
        QVERIFY(captionAt(book.captions, 2) == nullptr);
        QCOMPARE(captionAt(book.captions, 3)->text, book.captions[1].text);
        QVERIFY(captionAt(book.captions, 5.5) == nullptr);
        QCOMPARE(chapterAt(book.chapters, 2.999), 0);
        QCOMPARE(chapterAt(book.chapters, 3), 1);
    }
    void chapterTextIsNotACaption()
    {
        const auto book = MetadataReader::read(QString(FIXTURE_DIR) + "/no-captions.m4b",
            std::make_shared<std::atomic_bool>(false), true);
        QVERIFY2(book.error.isEmpty(), qPrintable(book.error));
        QCOMPARE(book.chapters.size(), 2);
        QVERIFY(book.captions.isEmpty());
    }
    void longCaptionAndCancellation()
    {
        const auto book = MetadataReader::read(QString(FIXTURE_DIR) + "/long-captions.m4b",
            std::make_shared<std::atomic_bool>(false), true);
        QVERIFY2(book.error.isEmpty(), qPrintable(book.error));
        QVERIFY(!book.captions.isEmpty());
        QVERIFY(book.captions[0].text.size() > 300);
        const auto cancelled = MetadataReader::read("missing.m4b", std::make_shared<std::atomic_bool>(true), true);
        QVERIFY(cancelled.error.isEmpty());
        QVERIFY(cancelled.captions.isEmpty());
    }
    void captionScrollAndRepeatedText()
    {
        const QVector<CaptionCue> cues{{2, 6, "same"}, {6, 10, "same"}};
        QCOMPARE(captionAt(cues, 5)->start, 2.0);
        QCOMPARE(captionAt(cues, 6)->start, 6.0);
        QVERIFY(captionAt(cues, 10) == nullptr);
        QVERIFY(captionAt(cues, std::numeric_limits<double>::quiet_NaN()) == nullptr);
        const CaptionCue longCue{30, 42, "long"};
        QCOMPARE(longCue.scrollFraction(31), 0.0);
        QCOMPARE(longCue.scrollFraction(32), 0.0);
        QCOMPARE(longCue.scrollFraction(36), 0.5);
        QCOMPARE(longCue.scrollFraction(40), 1.0);
        const CaptionCue shortCue{10, 15, "short"};
        QCOMPARE(shortCue.scrollFraction(11), 0.0);
        QCOMPARE(shortCue.scrollFraction(12.5), 0.5);
        QCOMPARE(shortCue.scrollFraction(14), 1.0);
    }
    void tx3gTextPayloads()
    {
        QCOMPARE(decodeMovText(QByteArray::fromHex("0002486900000008")), "Hi");
        QCOMPARE(decodeMovText(QByteArray::fromHex("0004feff0041")), "A");
        QCOMPARE(decodeMovText(QByteArray::fromHex("0004fffe4100")), "A");
        QVERIFY(decodeMovText(QByteArray::fromHex("000341")).isEmpty());
        QVERIFY(decodeMovText(QByteArray::fromHex("00")).isEmpty());
    }
    void identitySurvivesRenaming()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto first = directory.filePath("first.m4b");
        const auto copy = directory.filePath("copy.m4b");
        QVERIFY(QFile::copy(QString(FIXTURE_DIR) + "/captions.m4b", first));
        QVERIFY(QFile::copy(first, copy));
        QCOMPARE(quickFingerprint(first), quickFingerprint(copy));
        QCOMPARE(quickFingerprint(first), QString("quick:v1:f787a2ab090c09ff225c3b5ac5b5a159a2674a5fa0bdfb7d8aeec1c32377b160"));
        Library library(directory.filePath("library.sqlite"));
        const auto identity = quickFingerprint(first);
        library.saveProgress(identity, first, "First", "Author", 4.0);
        library.savePreferences(identity, true, true);
        library.saveProgress(identity, copy, "Renamed", "Author", 4.5);
        const auto state = library.find(quickFingerprint(copy));
        QCOMPARE(state.position, 4.5);
        QCOMPARE(state.captions.value(), true);
        QCOMPARE(state.stopAfterChapter.value(), true);
        QCOMPARE(library.recentBooks().size(), 2);
        QVERIFY(QFile::remove(copy));
        QCOMPARE(library.lastAvailablePath(), first);
    }
    void persistenceAndUnsetPreferences()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("library.sqlite");
        {
            Library library(path);
            library.saveProgress("book", "path", "Title", "Author", 25.0);
            const auto original = library.find("book");
            QVERIFY(!original.captions.has_value());
            QVERIFY(!original.stopAfterChapter.has_value());
            library.savePreferences("book", true, std::nullopt);
            QVERIFY(!library.find("book").stopAfterChapter.has_value());
            library.savePreferences("book", true, false);
            library.saveProgress("book", "path", "Title", "Author", 30.0);
        }
        Library reopened(path);
        const auto restored = reopened.find("book");
        QCOMPARE(restored.position, 30.0);
        QCOMPARE(restored.captions.value(), true);
        QCOMPARE(restored.stopAfterChapter.value(), false);
        QVERIFY(!reopened.sqliteVersion().isEmpty());
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
