#include "TiltEqualizer.h"
#include "LoudnessReader.h"
#include <QtTest>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <mpv/client.h>
#include <memory>
#include <cstring>

class EqualizerTests : public QObject {
    Q_OBJECT
    static QByteArray render(const QString &input, const QString &filter, const QString &output,
                             double speed = 1, int channels = 1)
    {
        std::unique_ptr<mpv_handle, decltype(&mpv_terminate_destroy)> handle(mpv_create(), mpv_terminate_destroy);
        if (!handle) return {};
        const auto option = [&](const char *name, const QByteArray &value) {
            return mpv_set_option_string(handle.get(), name, value.constData()) >= 0;
        };
        if (!option("config", "no") || !option("vo", "null") || !option("video", "no")
            || !option("ao", "pcm") || !option("ao-pcm-file", output.toUtf8())
            || !option("ao-pcm-waveheader", "no") || !option("audio-format", "float")
            || !option("audio-channels", channels == 1 ? "mono" : "stereo")
            || !option("audio-samplerate", "48000") || !option("audio-pitch-correction", "yes")
            || !option("speed", QByteArray::number(speed))
            || (!filter.isEmpty() && !option("af", filter.toUtf8()))
            || mpv_initialize(handle.get()) < 0) return {};
        const QByteArray path = input.toUtf8();
        const char *args[] = {"loadfile", path.constData(), nullptr};
        if (mpv_command(handle.get(), args) < 0) return {};
        QElapsedTimer timer; timer.start();
        bool finished = false;
        while (timer.elapsed() < 10000) {
            const auto *event = mpv_wait_event(handle.get(), 0.1);
            if (event->event_id == MPV_EVENT_END_FILE) {
                const auto *end = static_cast<mpv_event_end_file *>(event->data);
                finished = end->reason == MPV_END_FILE_REASON_EOF;
                break;
            }
        }
        handle.reset();
        if (!finished) return {};
        QFile result(output);
        if (!result.open(QIODevice::ReadOnly)) return {};
        return result.readAll();
    }
    static void header(QDataStream &wav, int frames, int rate, int channels)
    {
        wav.setByteOrder(QDataStream::LittleEndian);
        const int bytes = frames*channels*2;
        wav.writeRawData("RIFF", 4); wav << quint32(36+bytes);
        wav.writeRawData("WAVEfmt ", 8); wav << quint32(16) << quint16(1) << quint16(channels)
            << quint32(rate) << quint32(rate*channels*2) << quint16(channels*2) << quint16(16);
        wav.writeRawData("data", 4); wav << quint32(bytes);
    }
    static float sample(const QByteArray &data, int index)
    {
        float value;
        std::memcpy(&value, data.constData()+index*sizeof(float), sizeof(float));
        return value;
    }
private slots:
    void chapterLoudnessExcludesNeighbouringAudioAndIncludesToneHeadroom()
    {
        QTemporaryDir directory;
        const QString input = directory.filePath("two-chapters.wav");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);
        constexpr int rate = 16000;
        header(wav, 130*rate, rate, 1);
        for (int i = 0; i < 130*rate; ++i) {
            const double amplitude = i < 65*rate ? 0.04 : 0.4;
            wav << qint16(32767*amplitude*std::sin(2*std::acos(-1.0)*1000*i/rate));
        }
        file.close();
        auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto quiet = LoudnessReader::read(input, 0, 65, 0, 1000, cancel);
        const auto loud = LoudnessReader::read(input, 65, 130, 0, 1000, cancel);
        const auto tone = LoudnessReader::read(input, 0, 65, 12, 1000, cancel);
        QVERIFY2(quiet.error.isEmpty(), qPrintable(quiet.error));
        QVERIFY2(loud.error.isEmpty(), qPrintable(loud.error));
        QVERIFY2(tone.error.isEmpty(), qPrintable(tone.error));
        QVERIFY2(std::abs(quiet.integrated - (20*std::log10(0.04)-3.01)) < 0.2,
                 qPrintable(QString::number(quiet.integrated)));
        QVERIFY(std::abs(loud.integrated-quiet.integrated-20) < 0.05);
        QVERIFY(std::abs(quiet.integrated-tone.integrated-12) < 0.05);
        QVERIFY(!LoudnessReader::read(input, 0, 60, 0, 1000, cancel).error.isEmpty());
        QVERIFY(!LoudnessReader::read(input, 129, 194, 0, 1000, cancel).error.isEmpty());
        cancel->store(true);
        QVERIFY(LoudnessReader::read(input, 0, 65, 0, 1000, cancel).cancelled);
    }
    void silenceDoesNotProduceAnAutomaticBoost()
    {
        QTemporaryDir directory;
        const QString input = directory.filePath("silence.wav");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);
        header(wav, 65*16000, 16000, 1);
        file.write(QByteArray(65*16000*2, '\0'));
        file.close();
        const auto result = LoudnessReader::read(input, 0, 65, 0, 1000, std::make_shared<std::atomic_bool>(false));
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!std::isfinite(result.integrated));
    }
    void boostRaisesQuietPassagesWithoutHardClippingLoudOnes()
    {
        QTemporaryDir directory;
        const QString input = directory.filePath("quiet-and-loud.wav");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);
        header(wav, 3*48000, 48000, 1);
        for (int i = 0; i < 3*48000; ++i) {
            const double amplitude = i < 12000 ? 0 : i < 48000 || i >= 96000 ? 0.01 : 0.8;
            wav << qint16(32767*amplitude*std::sin(2*std::acos(-1.0)*1000*i/48000));
        }
        file.close();
        const auto flat = render(input, {}, directory.filePath("flat.raw"));
        const auto boosted = render(input, TiltEqualizer::filter(0, 1000, 24), directory.filePath("boosted.raw"));
        QVERIFY(!flat.isEmpty());
        // Resampling may round by one frame, but must not truncate the 10 ms
        // lookahead tail (480 frames at the output rate).
        QVERIFY(std::abs(boosted.size()-flat.size()) <= int(sizeof(float)));
        double quietOriginal = 0, quietBoosted = 0, peak = 0;
        double sine = 0, cosine = 0, loudPower = 0;
        for (int i = 0; i < boosted.size()/int(sizeof(float)); ++i) {
            const double value = sample(boosted, i);
            QVERIFY(std::isfinite(value));
            peak = std::max(peak, std::abs(value));
            if (i < 9600) QCOMPARE(value, 0.0);
            if (i >= 24000 && i < 38400) {
                quietOriginal += std::pow(sample(flat, i), 2);
                quietBoosted += value*value;
            }
            if (i >= 62400 && i < 86400) {
                const double phase = 2*std::acos(-1.0)*1000*i/48000;
                sine += value*std::sin(phase); cosine += value*std::cos(phase);
                loudPower += value*value;
            }
        }
        const double measuredBoost = 10*std::log10(quietBoosted/quietOriginal);
        QVERIFY2(std::abs(measuredBoost-24) < 0.1, qPrintable(QString::number(measuredBoost)));
        QVERIFY2(peak < 0.99 && peak > 0.8, qPrintable(QString::number(peak)));
        // A clipped sine has substantial extra harmonic energy. Lookahead gain
        // reduction should retain the steady sine instead of flattening peaks.
        const double fundamentalPower = 2*(sine*sine+cosine*cosine)/24000;
        const double distortion = std::sqrt(std::max(0.0, (loudPower-fundamentalPower)/loudPower));
        QVERIFY2(distortion < 0.02, qPrintable(QString::number(distortion)));
    }
    void maximumBoostProtectsStereoPeaksAtDifferentRatesAndSpeeds()
    {
        QTemporaryDir directory;
        struct Scenario { int rate; double speed, tone; };
        int scenario = 0;
        for (const auto &s : {Scenario{44100, 0.75, 12}, Scenario{48000, 1, 0}, Scenario{48000, 2, -12}}) {
            const QString input = directory.filePath(QString("stress-%1.wav").arg(scenario));
            QFile file(input);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QDataStream wav(&file);
            header(wav, 2*s.rate, s.rate, 2);
            for (int i = 0; i < 2*s.rate; ++i) {
                const double phase = 2*std::acos(-1.0)*i/s.rate;
                double value = (i / (s.rate/10)) % 2 ? 0.28 : 0.003;
                value *= std::sin(137*phase)+std::sin(997*phase)+std::sin(6000*phase);
                if (i % 997 == 0) value = 0.99;
                wav << qint16(value*32767) << qint16(value*32767*0.25);
            }
            file.close();
            const auto output = render(input, TiltEqualizer::filter(s.tone, 1000, 24),
                                       directory.filePath(QString("stress-%1.raw").arg(scenario++)), s.speed, 2);
            QVERIFY(!output.isEmpty());
            const double duration = output.size() / (48000.0*2*sizeof(float));
            QVERIFY2(std::abs(duration-2/s.speed) < 0.1, qPrintable(QString::number(duration)));
            double peak = 0, stereoError = 0, power = 0;
            for (int i = 0; i < output.size()/int(sizeof(float)); i += 2) {
                const double left = sample(output, i), right = sample(output, i+1);
                QVERIFY(std::isfinite(left) && std::isfinite(right));
                peak = std::max({peak, std::abs(left), std::abs(right)});
                stereoError += std::pow(right-left*0.25, 2);
                power += left*left;
            }
            QVERIFY2(peak < 1.0, qPrintable(QString::number(peak)));
            QVERIFY(peak > 0.2);
            // Both channels must share gain reduction, preserving stereo balance.
            QVERIFY(std::sqrt(stereoError/power) < 0.005);
        }
    }
    void referenceResponseAndHeadroom()
    {
        for (double tone : {-12.0, -6.0, 0.0, 6.0, 12.0}) {
            for (double pivot : {200.0, 1000.0, 4000.0}) {
                const auto c = TiltEqualizer::coefficients(tone, pivot, false);
                QVERIFY(std::abs(c.a1) < 1);
                QVERIFY(std::abs(c.decibels(pivot)) < 1e-8);
                QVERIFY(std::abs(c.decibels(0) + tone) < 1e-8);
                QVERIFY(std::abs(c.decibels(24000) - tone) < 1e-8);
                const auto trimmed = TiltEqualizer::coefficients(tone, pivot);
                for (int i = 0; i <= 240; ++i)
                    QVERIFY(trimmed.decibels(i*100.0) < 1e-8);
            }
        }
    }
    void libmpvActuallyAppliesTheFilterToAudio()
    {
        QTemporaryDir directory;
        const QString input = directory.filePath("synthetic.wav");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);
        wav.setByteOrder(QDataStream::LittleEndian);
        constexpr int samples = 48000;
        wav.writeRawData("RIFF", 4); wav << quint32(36 + samples*2);
        wav.writeRawData("WAVEfmt ", 8); wav << quint32(16) << quint16(1) << quint16(1)
            << quint32(48000) << quint32(96000) << quint16(2) << quint16(16);
        wav.writeRawData("data", 4); wav << quint32(samples*2);
        for (int i = 0; i < samples; ++i) {
            const double phase = 2*std::acos(-1.0)*i/48000;
            wav << qint16(4000*(std::sin(100*phase) + std::sin(10000*phase)));
        }
        file.close();
        const auto flat = render(input, {}, directory.filePath("flat.raw"));
        const auto filtered = render(input, TiltEqualizer::filter(9, 1000), directory.filePath("tilt.raw"));
        QVERIFY2(flat.size() >= samples*int(sizeof(float)), "libmpv did not render baseline PCM");
        QCOMPARE(filtered.size(), flat.size());
        const auto amplitude = [](const QByteArray &data, double frequency) {
            double real = 0, imaginary = 0;
            // Ignore the first 100 ms so the IIR startup transient has settled.
            for (int i = 4800; i < 48000; ++i) {
                float sample;
                std::memcpy(&sample, data.constData()+i*sizeof(float), sizeof(float));
                const double phase = 2*std::acos(-1.0)*frequency*i/48000;
                real += sample*std::cos(phase); imaginary += sample*std::sin(phase);
            }
            return std::hypot(real, imaginary);
        };
        const auto expected = TiltEqualizer::coefficients(9, 1000);
        for (double frequency : {100.0, 10000.0}) {
            const double measured = 20*std::log10(amplitude(filtered, frequency)/amplitude(flat, frequency));
            QVERIFY2(std::abs(measured-expected.decibels(frequency)) < 0.05, qPrintable(QString::number(measured)));
        }
    }
};
QTEST_GUILESS_MAIN(EqualizerTests)
#include "EqualizerTests.moc"
