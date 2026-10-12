#include "Library.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>

namespace {
void check(bool success, const QSqlQuery &query)
{
    if (!success)
        throw std::runtime_error(query.lastError().text().toStdString());
}
}

Library::Library(const QString &databasePath)
    : m_connection(QUuid::createUuid().toString()),
      m_database(QSqlDatabase::addDatabase("QSQLITE", m_connection))
{
    try {
        if (databasePath != ":memory:" && !QDir().mkpath(QFileInfo(databasePath).absolutePath()))
            throw std::runtime_error("Cannot create the library directory");
        m_database.setDatabaseName(databasePath);
        if (!m_database.open())
            throw std::runtime_error(m_database.lastError().text().toStdString());
        QSqlQuery query(m_database);
        check(query.exec("PRAGMA foreign_keys = ON"), query);
        check(query.exec("PRAGMA busy_timeout = 5000"), query);
        check(query.exec("PRAGMA user_version"), query);
        query.next();
        const int version = query.value(0).toInt();
        if (version > 1)
            throw std::runtime_error("Library was created by a newer version of qt-m4b-player");
        if (version == 0) {
            if (!m_database.transaction())
                throw std::runtime_error(m_database.lastError().text().toStdString());
            try {
                check(query.exec("CREATE TABLE books (fingerprint TEXT PRIMARY KEY, title TEXT NOT NULL, "
                    "author TEXT NOT NULL, position REAL NOT NULL DEFAULT 0, last_opened INTEGER NOT NULL, "
                    "captions INTEGER, stop_after_chapter INTEGER, rating INTEGER NOT NULL DEFAULT 0, "
                    "notes TEXT NOT NULL DEFAULT '')"), query);
                check(query.exec("CREATE TABLE book_locations (path TEXT PRIMARY KEY, "
                    "fingerprint TEXT NOT NULL REFERENCES books(fingerprint), last_seen INTEGER NOT NULL)"), query);
                check(query.exec("PRAGMA user_version = 1"), query);
                if (!m_database.commit())
                    throw std::runtime_error(m_database.lastError().text().toStdString());
            } catch (...) {
                m_database.rollback();
                throw;
            }
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
    if (!m_database.transaction())
        throw std::runtime_error(m_database.lastError().text().toStdString());
    try {
        QSqlQuery query(m_database);
        query.prepare("INSERT INTO books (fingerprint, title, author, position, last_opened) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(fingerprint) DO UPDATE SET title=excluded.title, author=excluded.author, "
            "position=excluded.position, last_opened=excluded.last_opened");
        for (const auto &value : QVariantList{fingerprint, title, author, position, QDateTime::currentMSecsSinceEpoch()})
            query.addBindValue(value);
        check(query.exec(), query);
        query.prepare("INSERT INTO book_locations (path, fingerprint, last_seen) VALUES (?, ?, ?) "
            "ON CONFLICT(path) DO UPDATE SET fingerprint=excluded.fingerprint, last_seen=excluded.last_seen");
        query.addBindValue(path);
        query.addBindValue(fingerprint);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        check(query.exec(), query);
        if (!m_database.commit())
            throw std::runtime_error(m_database.lastError().text().toStdString());
    } catch (...) {
        m_database.rollback();
        throw;
    }
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
    check(query.exec("SELECT b.title, b.author, l.path FROM books b JOIN book_locations l USING(fingerprint) "
        "ORDER BY b.last_opened DESC, l.last_seen DESC LIMIT 30"), query);
    QVariantList result;
    while (query.next())
        result.push_back(QVariantMap{{"title", query.value(0)}, {"author", query.value(1)}, {"path", query.value(2)}});
    return result;
}

QString Library::lastAvailablePath() const
{
    for (const auto &book : recentBooks()) {
        const QString path = book.toMap()["path"].toString();
        if (QFileInfo(path).isFile())
            return path;
    }
    return {};
}

QString Library::sqliteVersion() const
{
    QSqlQuery query(m_database);
    check(query.exec("SELECT sqlite_version()"), query);
    query.next();
    return query.value(0).toString();
}
