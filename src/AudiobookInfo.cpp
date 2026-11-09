#include "AudiobookInfo.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QVariantList>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace {
const QString unknown = QStringLiteral("Unknown");

// ISO 8601 local time. Locale long formats spell weekday and month names from
// LC_TIME, which need not match the desktop's display language.
QString date(const QDateTime &time)
{
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : unknown;
}

QString codecName(AVCodecID id)
{
    const auto *descriptor = avcodec_descriptor_get(id);
    if (!descriptor)
        return unknown;
    const QString name = QString::fromUtf8(descriptor->name);
    return descriptor->long_name ? QString::fromUtf8(descriptor->long_name) + " (" + name + ")" : name;
}

QString languageName(const QString &code)
{
    if (code.isEmpty() || code == "und")
        return unknown;
    const auto language = QLocale::codeToLanguage(code, QLocale::AnyLanguageCode);
    if (language == QLocale::AnyLanguage || language == QLocale::C)
        return code;
    return QLocale::languageToString(language) + " (" + code + ")";
}

struct Input {
    AVFormatContext *context = avformat_alloc_context();
    ~Input() { avformat_close_input(&context); }
};
}

QVariantMap AudiobookInfo::read(const QString &path, const Cancellation &cancel)
{
    QVariantMap result;
    const QLocale locale;
    const QFileInfo file(path);
    if (!file.isFile()) {
        result["fileError"] = QStringLiteral("The file is no longer available");
    } else {
        result["fileName"] = file.fileName();
        result["path"] = file.absoluteFilePath();
        const QString kind = QMimeDatabase().mimeTypeForFile(file).comment();
        result["kind"] = kind.isEmpty() ? unknown : kind;
        result["size"] = locale.formattedDataSize(file.size()) + " ("
            + locale.toString(file.size()) + " bytes)";
        result["created"] = date(file.birthTime());
        result["modified"] = date(file.lastModified());
    }
    if (cancel->load())
        return result;

    Input input;
    QString error;
    if (!input.context) {
        error = QStringLiteral("Cannot allocate a media reader");
    } else {
        input.context->interrupt_callback = {
            [](void *value) { return static_cast<std::atomic_bool *>(value)->load() ? 1 : 0; }, cancel.get()};
        const QByteArray filename = QFile::encodeName(path);
        int status = avformat_open_input(&input.context, filename.constData(), nullptr, nullptr);
        if (status >= 0)
            status = avformat_find_stream_info(input.context, nullptr);
        if (status < 0) {
            char text[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(status, text, sizeof(text));
            error = QString::fromUtf8(text);
        }
    }
    if (cancel->load())
        return result;
    if (!error.isEmpty()) {
        result["trackError"] = error;
        return result;
    }
    QVariantList audio, subtitles;
    for (unsigned i = 0; i < input.context->nb_streams; ++i) {
        const auto *stream = input.context->streams[i];
        const auto *codec = stream->codecpar;
        if (codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            const int channels = codec->ch_layout.nb_channels;
            QString channelText = channels > 0 ? locale.toString(channels) : unknown;
            if (channels == 1) channelText += " (Mono)";
            else if (channels == 2) channelText += " (Stereo)";
            audio.append(QVariantMap{
                {"codec", codecName(codec->codec_id)},
                {"bitrate", codec->bit_rate > 0 ? locale.toString(qRound64(codec->bit_rate / 1000.0)) + " kb/s" : unknown},
                {"sampleRate", codec->sample_rate > 0 ? locale.toString(codec->sample_rate / 1000.0, 'f', 1) + " kHz" : unknown},
                {"channels", channelText},
            });
        } else if (codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            const auto *language = av_dict_get(stream->metadata, "language", nullptr, 0);
            subtitles.append(QVariantMap{
                {"index", int(i)},
                {"id", stream->id > 0 ? QString::number(stream->id) : QString::number(i + 1)},
                {"language", languageName(language ? QString::fromUtf8(language->value) : QString())},
                {"format", codecName(codec->codec_id)},
            });
        }
    }
    result["audioTracks"] = audio;
    result["subtitleTracks"] = subtitles;
    return result;
}
