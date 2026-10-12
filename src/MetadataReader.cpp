#include "MetadataReader.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <algorithm>
#include <cmath>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

namespace {
int interrupted(void *context)
{
    return static_cast<std::atomic_bool *>(context)->load();
}

QString tag(AVDictionary *metadata, const char *name)
{
    const auto *entry = av_dict_get(metadata, name, nullptr, 0);
    return entry ? QString::fromUtf8(entry->value) : QString();
}

QString avError(int code)
{
    char message[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, message, sizeof(message));
    return QString::fromUtf8(message);
}

struct Input {
    AVFormatContext *context = avformat_alloc_context();
    ~Input() { avformat_close_input(&context); }
};
}

MediaData MetadataReader::read(const QString &path, const Cancellation &cancel, bool indexCaptions)
{
    MediaData result;
    try {
        if (cancel->load())
            return result;
        Input input;
        if (!input.context)
            throw std::runtime_error("Cannot allocate a media reader");
        input.context->interrupt_callback = {interrupted, cancel.get()};
        const QByteArray filename = QFile::encodeName(path);
        int status = avformat_open_input(&input.context, filename.constData(), nullptr, nullptr);
        if (status < 0)
            throw std::runtime_error(avError(status).toStdString());
        status = avformat_find_stream_info(input.context, nullptr);
        if (status < 0)
            throw std::runtime_error(avError(status).toStdString());
        auto *format = input.context;
        result.duration = format->duration == AV_NOPTS_VALUE ? 0 : double(format->duration) / AV_TIME_BASE;
        result.title = tag(format->metadata, "title");
        if (result.title.isEmpty())
            result.title = QFileInfo(path).completeBaseName();
        result.author = tag(format->metadata, "artist");
        if (result.author.isEmpty())
            result.author = tag(format->metadata, "author");
        result.fingerprint = quickFingerprint(path);
        for (unsigned i = 0; i < format->nb_chapters; ++i) {
            const auto *chapter = format->chapters[i];
            const double start = chapter->start * av_q2d(chapter->time_base);
            const double end = chapter->end * av_q2d(chapter->time_base);
            if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start)
                continue;
            auto title = tag(chapter->metadata, "title");
            if (title.isEmpty())
                title = QString("Chapter %1").arg(i + 1);
            result.chapters.push_back({title, start, end});
        }
        std::sort(result.chapters.begin(), result.chapters.end(), [](const Chapter &a, const Chapter &b) {
            return a.start < b.start;
        });
        result.chapters.erase(std::unique(result.chapters.begin(), result.chapters.end(),
            [](const Chapter &a, const Chapter &b) { return a.start == b.start; }), result.chapters.end());
        for (qsizetype i = 0; i + 1 < result.chapters.size(); ++i)
            result.chapters[i].end = result.chapters[i + 1].start;
        if (result.chapters.isEmpty() && result.duration > 0)
            result.chapters.push_back({"Whole book", 0, result.duration});

        int subtitle = -1;
        bool preferredSubtitle = false;
        const auto language = QLocale().name().section('_', 0, 0);
        for (unsigned i = 0; i < format->nb_streams; ++i) {
            const auto *stream = format->streams[i];
            if ((stream->disposition & AV_DISPOSITION_ATTACHED_PIC) && result.cover.isEmpty())
                result.cover = QByteArray(reinterpret_cast<const char *>(stream->attached_pic.data),
                                          stream->attached_pic.size);
            if (stream->codecpar->codec_id == AV_CODEC_ID_MOV_TEXT) {
                const auto lang = tag(stream->metadata, "language");
                const bool preferred = QLocale(lang).language() == QLocale(language).language();
                if (subtitle < 0 || (preferred && !preferredSubtitle)) {
                    subtitle = int(i);
                    preferredSubtitle = preferred;
                }
            }
        }
        if (!indexCaptions || subtitle < 0 || cancel->load())
            return result;
        AVPacket *packet = av_packet_alloc();
        if (!packet)
            throw std::runtime_error("Cannot allocate caption packet");
        const std::unique_ptr<AVPacket, void (*)(AVPacket *)> ownedPacket(packet,
            [](AVPacket *p) { av_packet_free(&p); });
        const double origin = format->start_time == AV_NOPTS_VALUE ? 0 : double(format->start_time) / AV_TIME_BASE;
        while (!cancel->load() && (status = av_read_frame(format, packet)) >= 0) {
            if (packet->stream_index == subtitle && packet->pts != AV_NOPTS_VALUE && packet->duration > 0) {
                const double scale = av_q2d(format->streams[subtitle]->time_base);
                const auto text = decodeMovText(QByteArray(reinterpret_cast<const char *>(packet->data), packet->size));
                const double start = packet->pts * scale - origin;
                const double end = start + packet->duration * scale;
                if (!text.isEmpty() && std::isfinite(start) && std::isfinite(end) && end > start)
                    result.captions.push_back({start, end, text});
            }
            av_packet_unref(packet);
        }
        if (!cancel->load() && status != AVERROR_EOF)
            throw std::runtime_error(avError(status).toStdString());
        std::sort(result.captions.begin(), result.captions.end(), [](const CaptionCue &a, const CaptionCue &b) {
            return a.start < b.start;
        });
    } catch (const std::exception &error) {
        if (!cancel->load())
            result.error = QString::fromUtf8(error.what());
    }
    return result;
}
