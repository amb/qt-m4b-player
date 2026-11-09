#include "AudioDecode.h"

#include <QFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace {
struct Input {
    AVFormatContext *value = avformat_alloc_context();
    ~Input() { avformat_close_input(&value); }
};
struct Decoder {
    AVCodecContext *value = nullptr;
    ~Decoder() { avcodec_free_context(&value); }
};
struct Frame {
    AVFrame *value = av_frame_alloc();
    Frame() { if (!value) throw std::bad_alloc(); }
    ~Frame() { av_frame_free(&value); }
};
struct Packet {
    AVPacket *value = av_packet_alloc();
    Packet() { if (!value) throw std::bad_alloc(); }
    ~Packet() { av_packet_free(&value); }
};
}

void AudioDecode::check(int status)
{
    if (status >= 0)
        return;
    char text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(status, text, sizeof(text));
    throw std::runtime_error(text);
}

bool AudioDecode::decodeRange(const QString &path, double start, double end, const Cancellation &cancel,
                              const std::function<void(AVFrame *, int, int)> &sink)
{
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start)
        throw std::runtime_error("The selected audio range is empty");
    if (cancel->load())
        return false;
    Input input;
    if (!input.value)
        throw std::bad_alloc();
    input.value->interrupt_callback = {
        [](void *value) { return static_cast<std::atomic_bool *>(value)->load() ? 1 : 0; }, cancel.get()};
    const auto file = QFile::encodeName(path);
    check(avformat_open_input(&input.value, file.constData(), nullptr, nullptr));
    check(avformat_find_stream_info(input.value, nullptr));
    const AVCodec *codec = nullptr;
    const int streamIndex = av_find_best_stream(input.value, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    check(streamIndex);
    const auto *stream = input.value->streams[streamIndex];
    Decoder decoder;
    decoder.value = avcodec_alloc_context3(codec);
    if (!decoder.value)
        throw std::bad_alloc();
    check(avcodec_parameters_to_context(decoder.value, stream->codecpar));
    decoder.value->thread_count = 1;
    decoder.value->pkt_timebase = stream->time_base;
    check(avcodec_open2(decoder.value, codec, nullptr));
    const double origin = input.value->start_time == AV_NOPTS_VALUE ? 0
        : double(input.value->start_time)/AV_TIME_BASE;
    if (start > 0) {
        const int64_t target = int64_t((start+origin)*AV_TIME_BASE);
        check(avformat_seek_file(input.value, -1, INT64_MIN, target, target, AVSEEK_FLAG_BACKWARD));
    }
    Packet packet;
    Frame frame;
    bool complete = false;
    double nextTime = start > 0 ? std::numeric_limits<double>::quiet_NaN() : 0;
    const auto decode = [&] {
        int status = AVERROR(EAGAIN);
        while (!cancel->load() && !complete && (status = avcodec_receive_frame(decoder.value, frame.value)) >= 0) {
            auto *audio = frame.value;
            const double time = audio->best_effort_timestamp == AV_NOPTS_VALUE ? nextTime
                : audio->best_effort_timestamp*av_q2d(stream->time_base)-origin;
            if (!std::isfinite(time) || audio->sample_rate <= 0)
                throw std::runtime_error("Cannot determine the audio timing");
            nextTime = time + double(audio->nb_samples)/audio->sample_rate;
            if (time >= end) {
                complete = true;
            } else if (nextTime > start) {
                const int first = int(std::clamp(std::ceil((start-time)*audio->sample_rate-1e-7),
                                                 0.0, double(audio->nb_samples)));
                const int last = int(std::clamp(std::ceil((end-time)*audio->sample_rate-1e-7),
                                                0.0, double(audio->nb_samples)));
                if (last > first)
                    sink(audio, first, last-first);
                if (nextTime >= end) complete = true;
            }
            av_frame_unref(frame.value);
        }
        if (!cancel->load() && !complete && status != AVERROR(EAGAIN) && status != AVERROR_EOF)
            check(status);
    };
    int status = 0;
    while (!cancel->load() && !complete && (status = av_read_frame(input.value, packet.value)) >= 0) {
        if (packet.value->stream_index == streamIndex) {
            check(avcodec_send_packet(decoder.value, packet.value));
            decode();
        }
        av_packet_unref(packet.value);
    }
    if (cancel->load())
        return false;
    if (!complete) {
        if (status != AVERROR_EOF) check(status);
        check(avcodec_send_packet(decoder.value, nullptr));
        decode();
    }
    return !cancel->load();
}
