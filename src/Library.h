#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <optional>

struct BookState {
    double position = 0;
    std::optional<bool> captions;
    std::optional<bool> stopAfterChapter;
};

class Library {
public:
    explicit Library(const QString &databasePath);
    ~Library();
    Library(const Library &) = delete;
    Library &operator=(const Library &) = delete;
    BookState find(const QString &fingerprint) const;
    void saveProgress(const QString &fingerprint, const QString &path, const QString &title,
                      const QString &author, double position);
    void savePreferences(const QString &fingerprint, std::optional<bool> captions, std::optional<bool> stopAfterChapter);
    QVariantList recentBooks() const;
    QString lastAvailablePath() const;
    QString sqliteVersion() const;

private:
    QString m_connection;
    QSqlDatabase m_database;
};
