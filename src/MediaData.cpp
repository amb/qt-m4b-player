#include "MediaData.h"

#include <QCryptographicHash>
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>

double CaptionCue::scrollFraction(double time) const
{
    const double duration = end - start;
    if (!std::isfinite(time) || !std::isfinite(duration) || duration <= 0)
        return 0;
    const double hold = std::min(2.0, duration * 0.2);
    return std::clamp((time - start - hold) / (duration - 2 * hold), 0.0, 1.0);
}

int chapterAt(const QVector<Chapter> &chapters, double time)
{
    if (chapters.isEmpty() || !std::isfinite(time))
        return -1;
    const auto next = std::upper_bound(chapters.begin(), chapters.end(), time,
        [](double t, const Chapter &c) { return t < c.start; });
    return next == chapters.begin() ? 0 : int(next - chapters.begin()) - 1;
}

const CaptionCue *captionAt(const QVector<CaptionCue> &cues, double time)
{
    if (!std::isfinite(time))
        return nullptr;
    const auto next = std::upper_bound(cues.begin(), cues.end(), time,
        [](double t, const CaptionCue &c) { return t < c.start; });
    if (next == cues.begin())
        return nullptr;
    const auto &cue = *(next - 1);
    return time < cue.end ? &cue : nullptr;
}

QString quickFingerprint(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());
    const qint64 size = file.size();
    constexpr qint64 edge = 256 * 1024;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray("m4b-quick-fingerprint-v1\0", 25));
    const quint64 length = qToBigEndian(quint64(size));
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(&length), sizeof(length)));
    const auto first = file.read(std::min(size, edge));
    if (first.size() != std::min(size, edge) || !file.seek(std::max(qint64(0), size - edge)))
        throw std::runtime_error("Could not fingerprint the book");
    const auto last = file.read(std::min(size, edge));
    if (last.size() != std::min(size, edge))
        throw std::runtime_error("Could not fingerprint the book");
    hash.addData(first);
    hash.addData(last);
    return "quick:v1:" + QString::fromLatin1(hash.result().toHex());
}

QString decodeMovText(const QByteArray &packet)
{
    if (packet.size() < 2)
        return {};
    const auto count = qFromBigEndian<quint16>(packet.constData());
    if (count > packet.size() - 2)
        return {};
    const auto text = packet.mid(2, count);
    if (text.startsWith(QByteArray::fromHex("feff")) || text.startsWith(QByteArray::fromHex("fffe"))) {
        if (text.size() % 2)
            return {};
        const bool bigEndian = quint8(text[0]) == 0xfe;
        QString decoded;
        for (qsizetype i = 2; i < text.size(); i += 2)
            decoded += QChar(bigEndian ? qFromBigEndian<quint16>(text.constData() + i)
                                       : qFromLittleEndian<quint16>(text.constData() + i));
        return decoded;
    }
    return QString::fromUtf8(text);
}
