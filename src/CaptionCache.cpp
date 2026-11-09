#include "CaptionCache.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <cmath>

namespace {
constexpr quint32 magic = 0x4D344243; // "M4BC"
constexpr quint32 maximumCues = 10'000'000;
constexpr auto streamVersion = QDataStream::Qt_6_5;
}

QString CaptionCache::fileFor(const QString &directory, const QString &fingerprint)
{
    return QDir(directory).filePath(fingerprint.section(':', -1) + ".captions");
}

std::optional<CaptionCache::Entry> CaptionCache::load(const QString &directory, const QString &fingerprint)
{
    if (fingerprint.isEmpty())
        return std::nullopt;
    QFile file(fileFor(directory, fingerprint));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QDataStream in(&file);
    in.setVersion(streamVersion);
    quint32 fileMagic = 0, fileVersion = 0, count = 0;
    qint32 stream = -1;
    QString storedFingerprint;
    in >> fileMagic >> fileVersion >> storedFingerprint >> stream >> count;
    if (in.status() != QDataStream::Ok || fileMagic != magic || fileVersion != version
        || storedFingerprint != fingerprint || stream < 0 || count > maximumCues)
        return std::nullopt;
    Entry entry;
    entry.stream = stream;
    entry.cues.reserve(count);
    double previous = -1;
    for (quint32 i = 0; i < count; ++i) {
        CaptionCue cue;
        in >> cue.start >> cue.end >> cue.text;
        if (in.status() != QDataStream::Ok || !std::isfinite(cue.start) || !std::isfinite(cue.end)
            || cue.end <= cue.start || cue.start < previous || cue.text.isEmpty())
            return std::nullopt;
        previous = cue.start;
        entry.cues.push_back(std::move(cue));
    }
    if (!in.atEnd())
        return std::nullopt;
    return entry;
}

bool CaptionCache::save(const QString &directory, const QString &fingerprint, const Entry &entry)
{
    if (fingerprint.isEmpty() || entry.stream < 0 || quint64(entry.cues.size()) > maximumCues
        || !QDir().mkpath(directory))
        return false;
    QSaveFile file(fileFor(directory, fingerprint));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out.setVersion(streamVersion);
    out << magic << version << fingerprint << qint32(entry.stream) << quint32(entry.cues.size());
    for (const auto &cue : entry.cues)
        out << cue.start << cue.end << cue.text;
    return out.status() == QDataStream::Ok && file.commit();
}
