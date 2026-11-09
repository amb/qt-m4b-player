#include "AudioExporter.h"
#include "AudioDecode.h"

#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

namespace {
constexpr int headerSize = 44;
// RIFF and data chunk sizes are 32-bit fields.
constexpr quint64 maxDataBytes = 0xFFFFFFFFull - (headerSize - 8);

qint16 toPcm16(AVSampleFormat format, const uint8_t *data, qsizetype index)
{
    const auto fromFloat = [](double value) {
        return qint16(std::clamp(std::lround(value * 32768.0), -32768L, 32767L));
    };
    switch (format) {
    case AV_SAMPLE_FMT_U8: return qint16((int(data[index]) - 128) * 256);
    case AV_SAMPLE_FMT_S16: return reinterpret_cast<const qint16 *>(data)[index];
    case AV_SAMPLE_FMT_S32: return qint16(reinterpret_cast<const qint32 *>(data)[index] >> 16);
    case AV_SAMPLE_FMT_S64: return qint16(reinterpret_cast<const qint64 *>(data)[index] >> 48);
    case AV_SAMPLE_FMT_FLT: return fromFloat(reinterpret_cast<const float *>(data)[index]);
    case AV_SAMPLE_FMT_DBL: return fromFloat(reinterpret_cast<const double *>(data)[index]);
    default: throw std::runtime_error("The audio sample format is not supported for export");
    }
}

QByteArray wavHeader(int sampleRate, int channels, quint64 dataBytes)
{
    QByteArray header(headerSize, '\0');
    auto *out = reinterpret_cast<uchar *>(header.data());
    const auto put32 = [out](int offset, quint32 value) { qToLittleEndian(value, out + offset); };
    const auto put16 = [out](int offset, quint16 value) { qToLittleEndian(value, out + offset); };
    std::memcpy(out, "RIFF", 4);
    put32(4, quint32(dataBytes + headerSize - 8));
    std::memcpy(out + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1); // PCM
    put16(22, quint16(channels));
    put32(24, quint32(sampleRate));
    put32(28, quint32(sampleRate * channels * 2));
    put16(32, quint16(channels * 2));
    put16(34, 16);
    std::memcpy(out + 36, "data", 4);
    put32(40, quint32(dataBytes));
    return header;
}
}

QString AudioExporter::safeFileName(const QString &name)
{
    QString result;
    for (const QChar c : name)
        result += (c.unicode() < 0x20 || QStringLiteral("/\\:*?\"<>|").contains(c)) ? QChar('_') : c;
    result = result.simplified();
    while (result.startsWith('.'))
        result.remove(0, 1);
    return result.isEmpty() ? QStringLiteral("Audiobook") : result.left(200);
}

ExportResult AudioExporter::writeWav(const QString &source, double start, double end,
                                     const QString &destination, const Cancellation &cancel)
{
    ExportResult result;
    QSaveFile output(destination);
    try {
        if (!output.open(QIODevice::WriteOnly))
            throw std::runtime_error(output.errorString().toStdString());
        // Reserve the header; its sizes are filled in once the length is known.
        if (output.write(QByteArray(headerSize, '\0')) != headerSize)
            throw std::runtime_error(output.errorString().toStdString());
        QByteArray buffer;
        const bool finished = AudioDecode::decodeRange(source, start, end, cancel,
            [&](AVFrame *frame, int first, int count) {
                const int channels = frame->ch_layout.nb_channels;
                if (!result.sampleRate) {
                    if (frame->sample_rate <= 0 || channels <= 0 || channels > 32)
                        throw std::runtime_error("The audio format is not supported for export");
                    result.sampleRate = frame->sample_rate;
                    result.channels = channels;
                } else if (frame->sample_rate != result.sampleRate || channels != result.channels) {
                    throw std::runtime_error("The audio format changes within the exported range");
                }
                if ((result.frames + quint64(count)) * quint64(channels) * 2 > maxDataBytes)
                    throw std::runtime_error("The selected range is too long for a WAV file");
                const auto format = AVSampleFormat(frame->format);
                const auto packed = av_get_packed_sample_fmt(format);
                const bool planar = av_sample_fmt_is_planar(format);
                buffer.resize(qsizetype(count) * channels * 2);
                auto *out = reinterpret_cast<uchar *>(buffer.data());
                for (int sample = 0; sample < count; ++sample) {
                    for (int channel = 0; channel < channels; ++channel) {
                        const qint16 value = planar
                            ? toPcm16(packed, frame->extended_data[channel], first + sample)
                            : toPcm16(packed, frame->extended_data[0],
                                      qsizetype(first + sample) * channels + channel);
                        qToLittleEndian(value, out);
                        out += 2;
                    }
                }
                if (output.write(buffer) != buffer.size())
                    throw std::runtime_error(output.errorString().toStdString());
                result.frames += quint64(count);
            });
        if (!finished) {
            output.cancelWriting();
            result.cancelled = true;
            return result;
        }
        if (result.frames == 0)
            throw std::runtime_error("No audio could be decoded in the selected range");
        const quint64 dataBytes = result.frames * quint64(result.channels) * 2;
        if (!output.seek(0) || output.write(wavHeader(result.sampleRate, result.channels, dataBytes)) != headerSize)
            throw std::runtime_error(output.errorString().toStdString());
        if (!output.commit())
            throw std::runtime_error(output.errorString().toStdString());
    } catch (const std::exception &error) {
        output.cancelWriting();
        if (cancel->load())
            result.cancelled = true;
        else
            result.error = QString::fromUtf8(error.what());
    }
    return result;
}
