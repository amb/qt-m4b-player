#pragma once
#include "MediaData.h"

class MetadataReader {
public:
    // Metadata is quick; caption indexing runs separately after playback is ready.
    static MediaData read(const QString &path, const Cancellation &cancel, bool indexCaptions);
};
