#pragma once

#include "MediaData.h"
#include <QVariantMap>

class AudiobookInfo {
public:
    // Read-only file and track details for the Audiobook Info window, already
    // formatted for display. Runs on a worker; never modifies the file.
    //
    // Keys: fileError, fileName, path, kind, size, created, modified;
    // trackError, audioTracks [{codec, bitrate, sampleRate, channels}],
    // subtitleTracks [{index, id, language, format}]. Missing values are
    // "Unknown". An error key is non-empty when that part could not be read.
    static QVariantMap read(const QString &path, const Cancellation &cancel);
};
