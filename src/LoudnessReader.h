#pragma once

#include "MediaData.h"
#include <limits>

struct LoudnessResult {
    double integrated = std::numeric_limits<double>::quiet_NaN();
    QString error;
    bool cancelled = false;
};

class LoudnessReader {
public:
    // Decode only the selected chapter, apply tone/headroom but no Boost, and
    // measure gated integrated loudness using FFmpeg's BS.1770/EBU R128 meter.
    static LoudnessResult read(const QString &path, double start, double end,
                               double tone, double frequency, const Cancellation &cancel);
};
