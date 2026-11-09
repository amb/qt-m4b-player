#pragma once
#include "MediaData.h"

#include <functional>

class MetadataReader {
public:
    // Read metadata and, when indexCaptions is set, the selected caption track.
    static MediaData read(const QString &path, const Cancellation &cancel, bool indexCaptions);
    // Read metadata, hand it to metadataReady, then index captions from the same
    // open container. Opening an MP4 parses its sample tables, which is large for
    // long books, so the container is opened once. metadataReady runs on the
    // calling thread and may adjust the data; returning false skips indexing
    // (for example when the captions are already cached). It is not called when
    // reading metadata fails or is cancelled.
    static MediaData read(const QString &path, const Cancellation &cancel,
                          const std::function<bool(MediaData &)> &metadataReady);
};
