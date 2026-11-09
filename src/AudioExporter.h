#pragma once

#include "MediaData.h"

struct ExportResult {
    QString error;
    bool cancelled = false;
    quint64 frames = 0;
    int sampleRate = 0;
    int channels = 0;
};

class AudioExporter {
public:
    // Decode [start, end) of the source's audio track and write it as 16-bit
    // PCM WAV at the source sample rate and channel count. Playback speed,
    // volume and EQ are not applied. The destination is replaced atomically:
    // a failed or cancelled export leaves any existing file untouched.
    static ExportResult writeWav(const QString &source, double start, double end,
                                 const QString &destination, const Cancellation &cancel);
    // A filename without directory separators or characters that common
    // Linux/Windows/macOS filesystems reject.
    static QString safeFileName(const QString &name);
};
