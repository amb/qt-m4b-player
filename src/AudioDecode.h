#pragma once

#include "MediaData.h"
#include <functional>

struct AVFrame;

namespace AudioDecode {
// Throw std::runtime_error with FFmpeg's description of a negative status.
void check(int status);

// Decode the file's best audio stream and pass only the samples inside the
// half-open book-time range [start, end) to sink as (frame, first sample,
// sample count). Seeks near start first. Returns false when cancelled; throws
// std::runtime_error on demux/decode errors or errors thrown by sink.
bool decodeRange(const QString &path, double start, double end, const Cancellation &cancel,
                 const std::function<void(AVFrame *frame, int first, int count)> &sink);
}
