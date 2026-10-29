#include "LoudnessReader.h"
#include "TiltEqualizer.h"

#include <QFile>
#include <algorithm>
#include <cmath>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

namespace {
void check(int status)
{
    if (status >= 0)
        return;
    char text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(status, text, sizeof(text));
    throw std::runtime_error(text);
}
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
struct Meter {
    AVFilterGraph *graph = avfilter_graph_alloc();
    AVFilterContext *source = nullptr, *sink = nullptr;
    int inputRate = 0, inputFormat = -1, inputChannels = 0;
    int64_t samples = 0;
    double integrated = std::numeric_limits<double>::quiet_NaN();
    ~Meter() { avfilter_graph_free(&graph); }

    AVFilterContext *filter(const char *name, const char *id, const QByteArray &options = {})
    {
        const auto *definition = avfilter_get_by_name(name);
        if (!definition)
            throw std::runtime_error("The installed FFmpeg lacks the loudness measurement filters");
        AVFilterContext *result = nullptr;
        check(avfilter_graph_create_filter(&result, definition, id,
                                           options.isEmpty() ? nullptr : options.constData(), nullptr, graph));
        return result;
    }
    void initialize(const AVFrame *frame, double tone, double frequency)
    {
        if (!graph)
            throw std::bad_alloc();
        graph->nb_threads = 1;
        inputRate = frame->sample_rate;
        inputFormat = frame->format;
        inputChannels = frame->ch_layout.nb_channels;
        if (inputRate <= 0 || inputChannels <= 0)
            throw std::runtime_error("The chapter's audio format is not supported");
        const auto *buffer = avfilter_get_by_name("abuffer");
        if (!buffer)
            throw std::runtime_error("The installed FFmpeg lacks the audio buffer filter");
        source = avfilter_graph_alloc_filter(graph, buffer, "input");
        if (!source)
            throw std::bad_alloc();
        auto *parameters = av_buffersrc_parameters_alloc();
        if (!parameters)
            throw std::bad_alloc();
        parameters->format = frame->format;
        parameters->sample_rate = inputRate;
        parameters->time_base = {1, inputRate};
        int status = av_channel_layout_copy(&parameters->ch_layout, &frame->ch_layout);
        if (status >= 0)
            status = av_buffersrc_parameters_set(source, parameters);
        av_channel_layout_uninit(&parameters->ch_layout);
        av_free(parameters);
        check(status);
        check(avfilter_init_str(source, nullptr));
        auto *resample = filter("aresample", "rate", "48000");
        const auto c = TiltEqualizer::coefficients(tone, frequency);
        auto *equalizer = filter("biquad", "tone",
            QString("b0=%1:b1=%2:b2=0:a0=1:a1=%3:a2=0:precision=f64")
                .arg(c.b0, 0, 'g', 17).arg(c.b1, 0, 'g', 17).arg(c.a1, 0, 'g', 17).toUtf8());
        auto *meter = filter("ebur128", "meter", "metadata=1:framelog=verbose");
        // Read results from metadata; verbose per-frame reports stay below the
        // normal log threshold. Do not change FFmpeg's process-wide log callback.
        sink = filter("abuffersink", "output");
        check(avfilter_link(source, 0, resample, 0));
        check(avfilter_link(resample, 0, equalizer, 0));
        check(avfilter_link(equalizer, 0, meter, 0));
        check(avfilter_link(meter, 0, sink, 0));
        check(avfilter_graph_config(graph, nullptr));
    }
    void drain()
    {
        Frame output;
        int status;
        while ((status = av_buffersink_get_frame(sink, output.value)) >= 0) {
            if (const auto *entry = av_dict_get(output.value->metadata, "lavfi.r128.I", nullptr, 0)) {
                bool valid = false;
                const double value = QString::fromUtf8(entry->value).toDouble(&valid);
                if (valid && std::isfinite(value))
                    integrated = value;
            }
            av_frame_unref(output.value);
        }
        if (status != AVERROR(EAGAIN) && status != AVERROR_EOF)
            check(status);
    }
    void push(const AVFrame *frame, int first, int count, double tone, double frequency)
    {
        if (!source)
            initialize(frame, tone, frequency);
        if (frame->sample_rate != inputRate || frame->format != inputFormat
            || frame->ch_layout.nb_channels != inputChannels)
            throw std::runtime_error("The audio format changes within this chapter");
        Frame trimmed;
        trimmed.value->format = inputFormat;
        trimmed.value->sample_rate = inputRate;
        trimmed.value->nb_samples = count;
        trimmed.value->pts = samples;
        check(av_channel_layout_copy(&trimmed.value->ch_layout, &frame->ch_layout));
        check(av_frame_get_buffer(trimmed.value, 0));
        check(av_samples_copy(trimmed.value->extended_data, frame->extended_data, 0, first,
                              count, inputChannels, static_cast<AVSampleFormat>(inputFormat)));
        check(av_buffersrc_add_frame_flags(source, trimmed.value, AV_BUFFERSRC_FLAG_KEEP_REF));
        samples += count;
        drain();
    }
};
}

LoudnessResult LoudnessReader::read(const QString &path, double start, double end,
                                   double tone, double frequency, const Cancellation &cancel)
{
    LoudnessResult result;
    try {
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end-start <= 60)
            throw std::runtime_error("Select a chapter longer than one minute");
        if (cancel->load()) { result.cancelled = true; return result; }
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
        Meter meter;
        bool complete = false;
        double nextTime = start > 0 ? std::numeric_limits<double>::quiet_NaN() : 0;
        const auto decode = [&] {
            int status;
            while (!cancel->load() && !complete && (status = avcodec_receive_frame(decoder.value, frame.value)) >= 0) {
                // Raw mono/stereo streams can omit speaker labels. Filters
                // address named channels, so supply their conventional layout,
                // as playback does, rather than silently bypassing the tone EQ.
                if (frame.value->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
                    const int channels = frame.value->ch_layout.nb_channels;
                    if (channels < 1 || channels > 2)
                        throw std::runtime_error("Cannot determine this audio track's speaker layout");
                    av_channel_layout_uninit(&frame.value->ch_layout);
                    av_channel_layout_default(&frame.value->ch_layout, channels);
                }
                const auto *audio = frame.value;
                const double time = audio->best_effort_timestamp == AV_NOPTS_VALUE ? nextTime
                    : audio->best_effort_timestamp*av_q2d(stream->time_base)-origin;
                if (!std::isfinite(time) || audio->sample_rate <= 0)
                    throw std::runtime_error("Cannot determine the chapter's audio timing");
                nextTime = time + double(audio->nb_samples)/audio->sample_rate;
                if (time >= end) {
                    complete = true;
                } else if (nextTime > start) {
                    const int first = int(std::clamp(std::ceil((start-time)*audio->sample_rate-1e-7),
                                                     0.0, double(audio->nb_samples)));
                    const int last = int(std::clamp(std::ceil((end-time)*audio->sample_rate-1e-7),
                                                    0.0, double(audio->nb_samples)));
                    if (last > first)
                        meter.push(audio, first, last-first, tone, frequency);
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
        if (cancel->load()) { result.cancelled = true; return result; }
        if (!complete) {
            if (status != AVERROR_EOF) check(status);
            check(avcodec_send_packet(decoder.value, nullptr));
            decode();
        }
        if (cancel->load()) { result.cancelled = true; return result; }
        if (!meter.source || double(meter.samples)/meter.inputRate <= 60)
            throw std::runtime_error("This chapter has no more than one minute of measurable audio");
        check(av_buffersrc_add_frame_flags(meter.source, nullptr, 0));
        meter.drain();
        if (!std::isfinite(meter.integrated) || meter.integrated <= -69.9)
            throw std::runtime_error("This chapter is silent or too quiet to measure reliably");
        result.integrated = meter.integrated;
    } catch (const std::exception &error) {
        if (cancel->load())
            result.cancelled = true;
        else
            result.error = QString::fromUtf8(error.what());
    }
    return result;
}
