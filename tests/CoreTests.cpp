#include "Library.h"
#include "MediaData.h"
#include "MetadataReader.h"
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <QtTest>
#include <exception>
#include <limits>

namespace {

/// Second connection to the same file, used to inspect the schema and to prove
/// constraints that the Library API deliberately does not expose.
class RawDatabase {
public:
    explicit RawDatabase(const QString &path)
        : m_name(QUuid::createUuid().toString()),
          m_database(QSqlDatabase::addDatabase("QSQLITE", m_name))
    {
        m_database.setDatabaseName(path);
        m_open = m_database.open();
    }
    ~RawDatabase()
    {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }
    bool isOpen() const { return m_open; }
    bool exec(const QString &sql)
    {
        QSqlQuery query(m_database);
        return query.exec(sql);
    }
    QVariant scalar(const QString &sql)
    {
        QSqlQuery query(m_database);
        if (!query.exec(sql) || !query.next())
            return {};
        return query.value(0);
    }

private:
    QString m_name;
    QSqlDatabase m_database;
    bool m_open = false;
};

/// The version 1 schema exactly as it shipped, for upgrade testing.
bool createVersionOneLibrary(RawDatabase &raw)
{
    return raw.exec("CREATE TABLE books (fingerprint TEXT PRIMARY KEY, title TEXT NOT NULL, "
                    "author TEXT NOT NULL, position REAL NOT NULL DEFAULT 0, "
                    "last_opened INTEGER NOT NULL, captions INTEGER, stop_after_chapter INTEGER, "
                    "rating INTEGER NOT NULL DEFAULT 0, notes TEXT NOT NULL DEFAULT '')")
        && raw.exec("CREATE TABLE book_locations (path TEXT PRIMARY KEY, "
                    "fingerprint TEXT NOT NULL REFERENCES books(fingerprint), "
                    "last_seen INTEGER NOT NULL)")
        && raw.exec("PRAGMA user_version = 1");
}

} // namespace

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void ratingsPersistAndPreserveNotesAndProgress()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("library.sqlite");
        {
            Library library(path);
            library.saveProgress("a", "/books/a.m4b", "A", "Author", 42);
            library.savePreferences("a", true, false);
            library.saveNotes("a", "Keep my notes");
            library.saveRating("a", 35);
            library.saveProgress("a", "/books/renamed.m4b", "A", "Author", 43);
            QCOMPARE(library.find("a").rating, 35);
            QCOMPARE(library.find("a").position, 43.0);
            QVERIFY(library.find("a").captions.value());
            QCOMPARE(library.notes("a"), "Keep my notes");
            QVERIFY_EXCEPTION_THROWN(library.saveRating("a", 51), std::invalid_argument);
            QVERIFY_EXCEPTION_THROWN(library.saveRating("a", 1), std::invalid_argument);
            QVERIFY_EXCEPTION_THROWN(library.saveRating("missing", 30), std::runtime_error);
        }
        Library reopened(path);
        QCOMPARE(reopened.find("a").rating, 35);
        reopened.saveRating("a", 0);
        QCOMPARE(reopened.find("a").rating, 0);
        QCOMPARE(reopened.notes("a"), "Keep my notes");
    }
    void notesPersistWithoutOverwritingOtherBookFields()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("library.sqlite");
        {
            Library library(path);
            library.saveProgress("a", "/books/a.m4b", "A", "Author", 42);
            library.saveProgress("b", "/books/b.m4b", "B", "Author", 7);
            library.savePreferences("a", true, false);
            library.saveNotes("a", "A note — café\nSecond line <b>plain text</b>");
            library.saveNotes("b", "Another book");
            library.saveProgress("a", "/books/renamed.m4b", "A", "Author", 43);
            QCOMPARE(library.notes("a"), QString::fromUtf8("A note — café\nSecond line <b>plain text</b>"));
            QCOMPARE(library.notes("b"), "Another book");
            QCOMPARE(library.find("a").position, 43.0);
            QCOMPARE(library.find("a").captions.value(), true);
            QCOMPARE(library.find("a").stopAfterChapter.value(), false);
            QVERIFY_EXCEPTION_THROWN(library.saveNotes("missing", "draft"), std::runtime_error);
        }
        Library reopened(path);
        QVERIFY(reopened.notes("a").contains("Second line"));
        reopened.saveNotes("a", "");
        QVERIFY(reopened.notes("a").isEmpty());
        QCOMPARE(reopened.notes("b"), "Another book");
    }
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

        // One logical book with two known paths is a single recent entry, at
        // the most recently seen location.
        const auto recents = library.recentBooks();
        QCOMPARE(recents.size(), 1);
        QCOMPARE(recents.first().toMap()["path"].toString(), copy);

        // Removing that copy falls back to the other path of the same book.
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
    void missingMetadataIsAccepted()
    {
        QTemporaryDir directory;
        Library library(directory.filePath("library.sqlite"));

        // A book with no title or artist tag must save, not fail a NOT NULL check.
        library.saveProgress("quick:v1:bare", "/books/bare.m4b", QString(), QString(), 1.0);
        const auto initial = library.recentBooks().first().toMap();
        QCOMPARE(initial["title"].toString(), QString());
        QCOMPARE(initial["author"].toString(), QString());

        // Real metadata fills the gap, and a later open without it must not
        // erase what was learned earlier.
        library.saveProgress("quick:v1:bare", "/books/bare.m4b", "Real title", "Real author", 2.0);
        library.saveProgress("quick:v1:bare", "/books/bare.m4b", QString(), QString(), 3.0);
        const auto updated = library.recentBooks().first().toMap();
        QCOMPARE(updated["title"].toString(), QString("Real title"));
        QCOMPARE(updated["author"].toString(), QString("Real author"));
    }
    void migratesVersionOneLibrary()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("library.sqlite");
        {
            RawDatabase raw(path);
            QVERIFY(raw.isOpen());
            QVERIFY(createVersionOneLibrary(raw));
            QVERIFY(raw.exec("INSERT INTO books VALUES ('quick:v1:legacy', 'Old title', '', 42.5, "
                             "1700000000000, 1, NULL, 30, 'a note')"));
            QVERIFY(raw.exec("INSERT INTO book_locations VALUES ('/books/old.m4b', "
                             "'quick:v1:legacy', 1700000000001)"));
        }

        Library library(path);
        const auto state = library.find("quick:v1:legacy");
        QCOMPARE(state.position, 42.5);
        QCOMPARE(state.captions.value(), true);
        QVERIFY(!state.stopAfterChapter.has_value());

        const auto recents = library.recentBooks();
        QCOMPARE(recents.size(), 1);
        const auto book = recents.first().toMap();
        QCOMPARE(book["title"].toString(), QString("Old title"));
        QCOMPARE(book["author"].toString(), QString());
        QCOMPARE(book["path"].toString(), QString("/books/old.m4b"));

        QVERIFY(!QDir(directory.path()).entryList(QStringList{"library.sqlite.backup-before-migration-*"}).isEmpty());

        RawDatabase raw(path);
        QVERIFY(raw.isOpen());
        QCOMPARE(raw.scalar("PRAGMA user_version").toInt(), 2);
        QCOMPARE(raw.scalar("PRAGMA application_id").toInt(), 0x4D344251);
        QCOMPARE(raw.scalar("SELECT rating FROM books").toInt(), 30);
        QCOMPARE(raw.scalar("SELECT notes FROM books").toString(), QString("a note"));
        QVERIFY(raw.scalar("SELECT author IS NULL FROM books").toBool());
        QVERIFY(raw.scalar("SELECT sql FROM sqlite_master WHERE name = 'books'").toString().contains("STRICT"));
        QCOMPARE(raw.scalar("SELECT COUNT(*) FROM sqlite_master WHERE name IN "
                            "('books_last_opened', 'book_locations_fingerprint')").toInt(), 2);
    }
    void rejectsForeignDatabase()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("library.sqlite");
        {
            // Valid SQLite, valid user_version, but not our schema.
            RawDatabase raw(path);
            QVERIFY(raw.isOpen());
            QVERIFY(raw.exec("CREATE TABLE something (x TEXT)"));
            QVERIFY(raw.exec("PRAGMA user_version = 1"));
        }
        bool rejected = false;
        QString message;
        try {
            Library library(path);
        } catch (const std::exception &error) {
            rejected = true;
            message = QString::fromUtf8(error.what());
        }
        QVERIFY2(rejected, "a foreign SQLite file must not be opened as a library");
        QVERIFY2(message.contains("Not a qt-m4b-player library database"), qPrintable(message));

        RawDatabase raw(path);
        QVERIFY(raw.isOpen());
        QCOMPARE(raw.scalar("SELECT COUNT(*) FROM sqlite_master WHERE name = 'something'").toInt(), 1);
        QCOMPARE(raw.scalar("PRAGMA user_version").toInt(), 1);
    }
    void locationsCascadeAndConstraints()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("library.sqlite");
        {
            Library library(path);
            library.saveProgress("quick:v1:a", "/books/a.m4b", "A", "Author", 1.0);
        }
        {
            // Deleting the book removes its locations only with foreign keys on.
            RawDatabase raw(path);
            QVERIFY(raw.isOpen());
            QVERIFY(raw.exec("PRAGMA foreign_keys = ON"));
            QVERIFY(raw.exec("DELETE FROM books WHERE fingerprint = 'quick:v1:a'"));
            QCOMPARE(raw.scalar("SELECT COUNT(*) FROM book_locations").toInt(), 0);
        }
        {
            Library library(path);
            library.saveProgress("quick:v1:b", "/books/b.m4b", "B", "Author", 1.0);
        }
        {
            RawDatabase raw(path);
            QVERIFY(raw.isOpen());
            QVERIFY(!raw.exec("UPDATE books SET captions = 5"));
            QVERIFY(!raw.exec("UPDATE books SET rating = 99"));
            QVERIFY(!raw.exec("UPDATE books SET captions = 'yes'"));
            QVERIFY(raw.exec("UPDATE books SET captions = 0, rating = 50"));
        }
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
