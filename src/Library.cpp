#include "Library.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>

namespace {

/// Current on-disk schema. Bump this and add a step to the upgrade path below.
constexpr int kSchemaVersion = 2;

/// Identifies a database file as belonging to this application so that a
/// foreign SQLite file is rejected instead of silently migrated. "M4BQ".
constexpr int kApplicationId = 0x4D344251;

void check(bool success, const QSqlQuery &query)
{
    if (!success)
        throw std::runtime_error(query.lastError().text().toStdString());
}

// QSqlDatabase is a shared handle, so these helpers take it by value and still
// operate on the caller's connection.
void exec(QSqlDatabase database, const QString &sql)
{
    QSqlQuery query(database);
    check(query.exec(sql), query);
}

QVariant scalar(QSqlDatabase database, const QString &sql)
{
    QSqlQuery query(database);
    check(query.exec(sql), query);
    return query.next() ? query.value(0) : QVariant();
}

/// Runs `body` in a transaction, rolling back and rethrowing on failure.
template <typename Body>
void inTransaction(QSqlDatabase database, Body &&body)
{
    if (!database.transaction())
        throw std::runtime_error(database.lastError().text().toStdString());
    try {
        body();
    } catch (...) {
        database.rollback();
        throw;
    }
    if (!database.commit())
        throw std::runtime_error(database.lastError().text().toStdString());
}

/// STRICT tables require SQLite 3.37. Without this check the user sees a
/// syntax error from the CREATE TABLE instead of an actionable message.
void requireStrictTables(QSqlDatabase database)
{
    const auto parts = scalar(database, "SELECT sqlite_version()").toString().split('.');
    const int major = parts.value(0).toInt();
    const int minor = parts.value(1).toInt();
    if (major < 3 || (major == 3 && minor < 37))
        throw std::runtime_error("This library schema needs SQLite 3.37 or newer for STRICT tables");
}

/// True when the file carries both tables the version 1 schema defined. Used
/// to tell a genuine pre-identifier library from an unrelated SQLite file that
/// happens to report the same user_version.
bool hasVersionOneTables(QSqlDatabase database)
{
    return scalar(database, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"
                            " AND name IN ('books', 'book_locations')").toInt() == 2;
}

/// `title`, `author` and `notes` are nullable: audiobooks without metadata must
/// not fail to save. `captions` and `stop_after_chapter` stay nullable because
/// NULL means "unset" and 0 means "explicitly disabled". `position` is
/// deliberately unchecked: it comes from the playback clock, and a transient
/// negative reading must not turn into a failed save.
QString booksSchema(const QString &table)
{
    return QStringLiteral(
        "CREATE TABLE %1 ("
        " fingerprint TEXT PRIMARY KEY,"
        " title TEXT,"
        " author TEXT,"
        " position REAL NOT NULL DEFAULT 0.0,"
        " rating INTEGER NOT NULL DEFAULT 0 CHECK (rating BETWEEN 0 AND 50),"
        " notes TEXT,"
        " captions INTEGER CHECK (captions IN (0, 1)),"
        " stop_after_chapter INTEGER CHECK (stop_after_chapter IN (0, 1)),"
        " last_opened INTEGER NOT NULL"
        ") STRICT").arg(table);
}

QString locationsSchema(const QString &table)
{
    return QStringLiteral(
        "CREATE TABLE %1 ("
        " path TEXT PRIMARY KEY,"
        " fingerprint TEXT NOT NULL REFERENCES books(fingerprint) ON DELETE CASCADE,"
        " last_seen INTEGER NOT NULL"
        ") STRICT").arg(table);
}

void createIndexes(QSqlDatabase database)
{
    exec(database, "CREATE INDEX books_last_opened ON books(last_opened DESC)");
    exec(database, "CREATE INDEX book_locations_fingerprint ON book_locations(fingerprint)");
}

void stampSchema(QSqlDatabase database)
{
    exec(database, QStringLiteral("PRAGMA application_id = %1").arg(kApplicationId));
    exec(database, QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
}

void createSchema(QSqlDatabase database)
{
    exec(database, booksSchema("books"));
    exec(database, locationsSchema("book_locations"));
    createIndexes(database);
}

/// Rebuilds the version 1 tables. STRICT and relaxed NOT NULL constraints
/// cannot be expressed with ALTER TABLE, so both tables are recreated.
void upgradeFromVersionOne(QSqlDatabase database)
{
    exec(database, booksSchema("books_v2"));
    exec(database,
        "INSERT INTO books_v2 (fingerprint, title, author, position, rating, notes,"
        " captions, stop_after_chapter, last_opened)"
        " SELECT fingerprint, NULLIF(title, ''), NULLIF(author, ''), position, rating,"
        " NULLIF(notes, ''), captions, stop_after_chapter, last_opened FROM books");

    // Hold the location rows in a table without foreign keys so that no
    // REFERENCES clause points at `books` while it is dropped and recreated.
    exec(database, "CREATE TABLE book_locations_hold AS"
                   " SELECT path, fingerprint, last_seen FROM book_locations");
    exec(database, "DROP TABLE book_locations");
    exec(database, "DROP TABLE books");
    exec(database, "ALTER TABLE books_v2 RENAME TO books");
    exec(database, locationsSchema("book_locations"));
    exec(database,
        "INSERT INTO book_locations (path, fingerprint, last_seen)"
        " SELECT path, fingerprint, last_seen FROM book_locations_hold");
    exec(database, "DROP TABLE book_locations_hold");
    createIndexes(database);

    QSqlQuery query(database);
    check(query.exec("PRAGMA foreign_key_check"), query);
    if (query.next())
        throw std::runtime_error("Library migration left book locations without a book");
}

/// Copies the database aside before an upgrade. Returns without touching the
/// filesystem when there is nothing to lose.
void backupBeforeMigration(QSqlDatabase database, const QString &databasePath)
{
    // Fold any write-ahead log into the main file so the copy is complete.
    QSqlQuery checkpoint(database);
    checkpoint.exec("PRAGMA wal_checkpoint(TRUNCATE)");

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString target = QStringLiteral("%1.backup-before-migration-%2").arg(databasePath, stamp);
    for (int attempt = 1; QFile::exists(target); ++attempt)
        target = QStringLiteral("%1.backup-before-migration-%2-%3").arg(databasePath, stamp).arg(attempt);
    if (!QFile::copy(databasePath, target))
        throw std::runtime_error("Cannot back up the library before upgrading it");
}

} // namespace

Library::Library(const QString &databasePath)
    : m_connection(QUuid::createUuid().toString()),
      m_database(QSqlDatabase::addDatabase("QSQLITE", m_connection))
{
    try {
        const bool inMemory = databasePath == ":memory:";
        if (!inMemory && !QDir().mkpath(QFileInfo(databasePath).absolutePath()))
            throw std::runtime_error("Cannot create the library directory");
        m_database.setDatabaseName(databasePath);
        if (!m_database.open())
            throw std::runtime_error(m_database.lastError().text().toStdString());

        exec(m_database, "PRAGMA foreign_keys = ON");
        exec(m_database, "PRAGMA busy_timeout = 5000");

        const int version = scalar(m_database, "PRAGMA user_version").toInt();
        if (version > kSchemaVersion)
            throw std::runtime_error("Library was created by a newer version of qt-m4b-player");
        if (version > 0) {
            const int identifier = scalar(m_database, "PRAGMA application_id").toInt();
            // Refuse to touch someone else's SQLite file. Only a version 1
            // library predates the identifier, and it must still look like one.
            const bool legacy = version == 1 && identifier == 0 && hasVersionOneTables(m_database);
            if (identifier != kApplicationId && !legacy)
                throw std::runtime_error("Not a qt-m4b-player library database");
        }

        // Back up before anything writes to the file, including the journal
        // mode change below, which is persistent.
        if (version > 0 && version < kSchemaVersion && !inMemory)
            backupBeforeMigration(m_database, databasePath);

        if (!inMemory) {
            exec(m_database, "PRAGMA journal_mode = WAL");
            // WAL keeps the database corruption-safe at synchronous = NORMAL;
            // only the most recent transaction can be lost on a hard reset.
            exec(m_database, "PRAGMA synchronous = NORMAL");
        }

        if (version == 0) {
            requireStrictTables(m_database);
            inTransaction(m_database, [&] {
                createSchema(m_database);
                stampSchema(m_database);
            });
        } else if (version < kSchemaVersion) {
            requireStrictTables(m_database);
            // Required around the rebuild: dropping `books` with foreign keys
            // enabled would cascade the location rows away.
            exec(m_database, "PRAGMA foreign_keys = OFF");
            inTransaction(m_database, [&] {
                upgradeFromVersionOne(m_database);
                stampSchema(m_database);
            });
            exec(m_database, "PRAGMA foreign_keys = ON");
        }
    } catch (...) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connection);
        throw;
    }
}

Library::~Library()
{
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}

BookState Library::find(const QString &fingerprint) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT position, captions, stop_after_chapter FROM books WHERE fingerprint = ?");
    query.addBindValue(fingerprint);
    check(query.exec(), query);
    BookState state;
    if (query.next()) {
        state.position = query.value(0).toDouble();
        if (!query.value(1).isNull())
            state.captions = query.value(1).toBool();
        if (!query.value(2).isNull())
            state.stopAfterChapter = query.value(2).toBool();
    }
    return state;
}

void Library::saveProgress(const QString &fingerprint, const QString &path, const QString &title,
                           const QString &author, double position)
{
    inTransaction(m_database, [&] {
        QSqlQuery query(m_database);
        // Absent metadata must not erase metadata recorded earlier: a later
        // open that cannot read a title keeps the known one.
        query.prepare("INSERT INTO books (fingerprint, title, author, position, last_opened)"
                      " VALUES (?, ?, ?, ?, ?)"
                      " ON CONFLICT(fingerprint) DO UPDATE SET"
                      " title = COALESCE(NULLIF(excluded.title, ''), books.title),"
                      " author = COALESCE(NULLIF(excluded.author, ''), books.author),"
                      " position = excluded.position,"
                      " last_opened = excluded.last_opened");
        query.addBindValue(fingerprint);
        query.addBindValue(title.isEmpty() ? QVariant() : QVariant(title));
        query.addBindValue(author.isEmpty() ? QVariant() : QVariant(author));
        query.addBindValue(position);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        check(query.exec(), query);

        query.prepare("INSERT INTO book_locations (path, fingerprint, last_seen) VALUES (?, ?, ?)"
                      " ON CONFLICT(path) DO UPDATE SET fingerprint = excluded.fingerprint,"
                      " last_seen = excluded.last_seen");
        query.addBindValue(path);
        query.addBindValue(fingerprint);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        check(query.exec(), query);
    });
}

void Library::savePreferences(const QString &fingerprint, std::optional<bool> captions, std::optional<bool> stopAfterChapter)
{
    QSqlQuery query(m_database);
    query.prepare("UPDATE books SET captions = COALESCE(?, captions), "
                  "stop_after_chapter = COALESCE(?, stop_after_chapter) WHERE fingerprint = ?");
    query.addBindValue(captions ? QVariant(*captions) : QVariant());
    query.addBindValue(stopAfterChapter ? QVariant(*stopAfterChapter) : QVariant());
    query.addBindValue(fingerprint);
    check(query.exec(), query);
}

QVariantList Library::recentBooks() const
{
    QSqlQuery query(m_database);
    // One entry per logical book, at its most recently seen location, so a book
    // known under two paths does not appear twice in the menu.
    check(query.exec("SELECT COALESCE(b.title, ''), COALESCE(b.author, ''), l.path"
                     " FROM books b JOIN book_locations l ON l.path = ("
                     "   SELECT path FROM book_locations WHERE fingerprint = b.fingerprint"
                     "   ORDER BY last_seen DESC, rowid DESC LIMIT 1)"
                     " ORDER BY b.last_opened DESC LIMIT 30"), query);
    QVariantList result;
    while (query.next())
        result.push_back(QVariantMap{{"title", query.value(0)}, {"author", query.value(1)}, {"path", query.value(2)}});
    return result;
}

QString Library::lastAvailablePath() const
{
    // Walks every known location of the most recently opened books so a deleted
    // copy falls back to another path of the same book.
    QSqlQuery query(m_database);
    check(query.exec("SELECT l.path FROM book_locations l JOIN books b ON b.fingerprint = l.fingerprint"
                     " ORDER BY b.last_opened DESC, l.last_seen DESC, l.rowid DESC"), query);
    while (query.next()) {
        const QString path = query.value(0).toString();
        if (QFileInfo(path).isFile())
            return path;
    }
    return {};
}

QString Library::sqliteVersion() const
{
    return scalar(m_database, "SELECT sqlite_version()").toString();
}
