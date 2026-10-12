#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

struct Chapter {
    QString title;
    double start = 0;
    double end = 0;
};

struct CaptionCue {
    double start = 0;
    double end = 0;
    QString text;
    double scrollFraction(double time) const;
};

using Cancellation = std::shared_ptr<std::atomic_bool>;

struct MediaData {
    QString title;
    QString author;
    QString fingerprint;
    double duration = 0;
    QVector<Chapter> chapters;
    QByteArray cover;
    QVector<CaptionCue> captions;
    QString error;
};

int chapterAt(const QVector<Chapter> &chapters, double time);
const CaptionCue *captionAt(const QVector<CaptionCue> &cues, double time);
QString quickFingerprint(const QString &path);
QString decodeMovText(const QByteArray &packet);
