#pragma once

#include "MediaData.h"
#include <optional>

// Indexed embedded captions, cached per book so reopening a book does not
// re-read the whole file (slow on network shares). Files are replaceable:
// a missing, unreadable or older-version file simply means "index again".
namespace CaptionCache {
struct Entry {
    int stream = -1;               // container stream index of the indexed track
    QVector<CaptionCue> cues;      // may be empty: the track had no usable cues
};
// Bump when the file layout or the indexing rules in MetadataReader change.
constexpr quint32 version = 1;
QString fileFor(const QString &directory, const QString &fingerprint);
std::optional<Entry> load(const QString &directory, const QString &fingerprint);
bool save(const QString &directory, const QString &fingerprint, const Entry &entry);
}
