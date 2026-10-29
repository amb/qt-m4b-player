#include "PlayerController.h"

#include "MetadataReader.h"
#include "TiltEqualizer.h"
#include "LoudnessReader.h"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>

PlayerController::PlayerController(Library *library, const QString &cacheDirectory, bool silent, QObject *parent)
    : QObject(parent), m_library(library), m_cacheDirectory(cacheDirectory), m_engine(silent)
{
    m_recents = m_library->recentBooks();
    connect(this, &PlayerController::stateChanged, this, [this] {
        if (chapterStart() != m_loudnessStart || chapterEnd() != m_loudnessEnd)
            invalidateLoudness();
    });
    m_equalizerUpdate.setSingleShot(true);
    m_equalizerUpdate.setInterval(60);
    connect(&m_equalizerUpdate, &QTimer::timeout, this, [this] {
        m_engine.setEqualizer(m_equalizerEnabled, m_equalizerTone, m_equalizerFrequency, m_equalizerBoost);
    });
    connect(&m_engine, &MpvEngine::equalizerFailed, this, [this](const QString &message) {
        m_equalizerEnabled = false;
        emit equalizerChanged();
        report("EQ could not be applied: " + message);
    });
    connect(&m_engine, &MpvEngine::failed, this, &PlayerController::report);
    connect(&m_engine, &MpvEngine::fileLoaded, this, [this](const QString &path) {
        if (QFileInfo(path).absoluteFilePath() != m_path)
            return;
        m_loaded = true;
        applyResume();
        if (m_captionsTouched || m_stopTouched)
            savePreferences();
        emit stateChanged();
    });
    connect(&m_engine, &MpvEngine::propertyChanged, this, [this](const QString &name, const QVariant &value) {
        if (!m_loaded)
            return;
        if (name == "time-pos") {
            const double time = value.toDouble();
            if (!std::isfinite(time))
                return;
            if (m_seekTarget) {
                if (std::abs(time - *m_seekTarget) > 0.15 && m_seekElapsed.elapsed() < 1000)
                    return;
                m_seekTarget.reset();
            }
            // Paused seeks retain their requested position. The decoded audio
            // clock can round below a chapter start even after an exact seek.
            // During playback only move forward; explicit seeks already set
            // m_position, including when the user deliberately seeks backward.
            if (!m_playing || time < m_position)
                return;
            const double previous = m_position;
            const double boundary = chapterEnd();
            m_position = std::clamp(time, 0.0, std::max(m_duration, time));
            // Prototype cutoff: observe traversal, pause, then position at the boundary.
            // Engine-side audible cutoff parity is a separate validation milestone.
            if (m_stopAfter && m_playing && boundary > previous && time >= boundary) {
                m_engine.setPaused(true);
                m_playing = false;
                seek(boundary);
            }
            m_currentChapter = chapterAt(m_chapters, m_position);
        } else if (name == "duration") {
            const double duration = value.toDouble();
            if (std::isfinite(duration) && duration > 0)
                m_duration = duration;
        } else if (name == "pause") {
            m_playing = !value.toBool();
            if (!m_playing)
                save();
        } else if (name == "speed") {
            m_speed = value.toDouble();
        } else if (name == "volume") {
            m_volume = value.toDouble();
        } else if (name == "eof-reached" && value.toBool()) {
            m_playing = false;
            m_position = m_duration;
            save();
        }
        emit stateChanged();
    });
    connect(&m_checkpoint, &QTimer::timeout, this, [this] { if (m_playing) save(); });
    m_checkpoint.start(30000);
}

PlayerController::~PlayerController()
{
    shutdown();
}

void PlayerController::report(const QString &message)
{
    m_error = message;
    emit stateChanged();
}

void PlayerController::clearError()
{
    m_error.clear();
    emit stateChanged();
}

QVariantList PlayerController::chapters() const
{
    QVariantList result;
    for (const auto &chapter : m_chapters)
        result.push_back(QVariantMap{{"title", chapter.title}, {"start", chapter.start}, {"end", chapter.end}});
    return result;
}

double PlayerController::chapterStart() const
{
    return m_currentChapter >= 0 && m_currentChapter < m_chapters.size() ? m_chapters[m_currentChapter].start : 0;
}

double PlayerController::chapterEnd() const
{
    return m_currentChapter >= 0 && m_currentChapter < m_chapters.size() ? m_chapters[m_currentChapter].end : m_duration;
}

QString PlayerController::captionText() const
{
    const auto *cue = m_captions ? captionAt(m_cues, m_position) : nullptr;
    return cue ? cue->text : QString();
}

double PlayerController::captionScroll() const
{
    const auto *cue = m_captions ? captionAt(m_cues, m_position) : nullptr;
    return cue ? cue->scrollFraction(m_position) : 0;
}

void PlayerController::openBook(const QUrl &url)
{
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) {
        report("Choose an available local audiobook file.");
        return;
    }
    cancelLoudness();
    m_loudnessStatus.clear();
    emit loudnessChanged();
    save();
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++m_generation;
    m_path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    m_fingerprint.clear();
    m_title = QFileInfo(m_path).completeBaseName();
    m_author.clear();
    m_cover = QUrl();
    m_error.clear();
    m_chapters.clear();
    m_cues.clear();
    m_chapterPositions.clear();
    m_currentChapter = -1;
    m_rating = 0;
    m_position = m_duration = m_resume = 0;
    m_loaded = m_playing = m_metadataReady = false;
    m_positionTouched = m_captionsTouched = m_stopTouched = false;
    m_captions = m_stopAfter = false;
    m_indexingCaptions = true;
    m_seekTarget.reset();
    if (!m_engine.load(m_path)) {
        m_cancel->store(true);
        m_indexingCaptions = false;
        emit stateChanged();
        return;
    }
    const auto path = m_path;
    const auto cancel = m_cancel;
    auto *watcher = new QFutureWatcher<MediaData>(this);
    connect(watcher, &QFutureWatcher<MediaData>::finished, this, [this, watcher, generation, path, cancel] {
        const auto data = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation || cancel->load())
            return;
        if (!data.error.isEmpty()) {
            m_indexingCaptions = false;
            report("Could not read the book: " + data.error);
            return;
        }
        m_title = data.title;
        m_author = data.author;
        if (data.duration > 0)
            m_duration = data.duration;
        m_chapters = data.chapters;
        m_currentChapter = chapterAt(m_chapters, m_position);
        m_fingerprint = data.fingerprint;
        try {
            const auto state = m_library->find(m_fingerprint);
            m_resume = state.position;
            m_rating = state.rating;
            if (!m_captionsTouched)
                m_captions = state.captions.value_or(false);
            if (!m_stopTouched)
                m_stopAfter = state.stopAfterChapter.value_or(false);
            m_metadataReady = true;
            applyResume();
            // Until the player is loaded, preserve stored progress rather than writing zero.
            if (m_loaded)
                save();
            if (m_loaded && (m_captionsTouched || m_stopTouched))
                savePreferences();
        } catch (const std::exception &error) {
            report("Library error: " + QString::fromUtf8(error.what()));
        }
        if (!data.cover.isEmpty()) {
            const QString coverPath = m_cacheDirectory + "/" + m_fingerprint.section(':', -1) + ".png";
            const auto image = QImage::fromData(data.cover);
            if (!image.isNull() && QDir().mkpath(m_cacheDirectory)) {
                QSaveFile file(coverPath);
                if (file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit())
                    m_cover = QUrl::fromLocalFile(coverPath);
            }
        }
        emit stateChanged();
        beginCaptionIndex(generation, path, cancel);
    });
    watcher->setFuture(QtConcurrent::run([path, cancel] { return MetadataReader::read(path, cancel, false); }));
    emit stateChanged();
}

void PlayerController::beginCaptionIndex(quint64 generation, const QString &path, const Cancellation &cancel)
{
    auto *watcher = new QFutureWatcher<MediaData>(this);
    connect(watcher, &QFutureWatcher<MediaData>::finished, this, [this, watcher, generation, cancel] {
        const auto data = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation || cancel->load())
            return;
        m_indexingCaptions = false;
        if (!data.error.isEmpty())
            report("Could not index captions: " + data.error);
        else
            m_cues = data.captions;
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([path, cancel] { return MetadataReader::read(path, cancel, true); }));
}

void PlayerController::applyResume()
{
    if (!m_loaded || !m_metadataReady)
        return;
    if (!m_positionTouched) {
        seek(std::clamp(m_resume, 0.0, m_duration));
        m_positionTouched = false;
    }
}

void PlayerController::save()
{
    if (!m_loaded || !m_metadataReady || m_fingerprint.isEmpty())
        return;
    try {
        m_library->saveProgress(m_fingerprint, m_path, m_title, m_author, m_position);
        m_recents = m_library->recentBooks();
    } catch (const std::exception &error) {
        report("Could not save listening progress: " + QString::fromUtf8(error.what()));
    }
}

void PlayerController::savePreferences()
{
    if (m_fingerprint.isEmpty())
        return;
    save();
    try {
        m_library->savePreferences(m_fingerprint,
            m_captionsTouched ? std::optional<bool>(m_captions) : std::nullopt,
            m_stopTouched ? std::optional<bool>(m_stopAfter) : std::nullopt);
    } catch (const std::exception &error) {
        report("Could not save playback preferences: " + QString::fromUtf8(error.what()));
    }
}

void PlayerController::togglePlayback()
{
    if (!m_loaded)
        return;
    m_positionTouched = true;
    if (!m_playing && m_position >= m_duration && m_duration > 0)
        seek(0);
    if (m_engine.setPaused(m_playing))
        m_playing = !m_playing;
    save();
    emit stateChanged();
}

void PlayerController::stop()
{
    if (!m_loaded)
        return;
    m_engine.setPaused(true);
    m_playing = false;
    seek(chapterStart());
}

void PlayerController::seek(double seconds)
{
    if (!m_loaded || !std::isfinite(seconds) || m_duration <= 0)
        return;
    const double target = std::clamp(seconds, 0.0, m_duration);
    if (!m_engine.seek(target))
        return;
    m_positionTouched = true;
    m_position = target;
    m_seekTarget = target;
    m_seekElapsed.start();
    m_currentChapter = chapterAt(m_chapters, target);
    save();
    emit stateChanged();
}

void PlayerController::skip(double seconds)
{
    seek(m_position + seconds);
}

void PlayerController::navigateChapter(int offset)
{
    const int source = m_currentChapter;
    const int target = source + offset;
    if (source < 0 || target < 0 || target >= m_chapters.size())
        return;
    m_chapterPositions[source] = std::clamp(m_position, m_chapters[source].start,
                                          std::nextafter(m_chapters[source].end, m_chapters[source].start));
    m_engine.setPaused(true);
    m_playing = false;
    seek(m_chapterPositions.value(target, m_chapters[target].start));
}

void PlayerController::selectChapter(int index)
{
    if (index >= 0 && index < m_chapters.size())
        seek(m_chapters[index].start);
}

void PlayerController::setSpeed(double speed)
{
    if (std::isfinite(speed) && speed >= 0.25 && speed <= 4 && m_engine.setSpeed(speed)) {
        m_speed = speed;
        emit stateChanged();
    }
}

void PlayerController::setVolume(double volume)
{
    if (std::isfinite(volume) && m_engine.setVolume(std::clamp(volume, 0.0, 100.0))) {
        m_volume = std::clamp(volume, 0.0, 100.0);
        emit stateChanged();
    }
}

void PlayerController::setCaptions(bool enabled)
{
    m_captionsTouched = true;
    m_captions = enabled;
    savePreferences();
    emit stateChanged();
}

void PlayerController::setStopAfterChapter(bool enabled)
{
    m_stopTouched = true;
    m_stopAfter = enabled;
    savePreferences();
    emit stateChanged();
}

void PlayerController::restoreLastBook()
{
    const QString path = m_library->lastAvailablePath();
    if (!path.isEmpty())
        openBook(QUrl::fromLocalFile(path));
}

void PlayerController::setRating(int rating)
{
    if (!notesAvailable())
        return;
    try {
        m_library->saveRating(m_fingerprint, rating);
        m_rating = rating;
        emit stateChanged();
    } catch (const std::exception &error) {
        report("Could not save rating: " + QString::fromUtf8(error.what()));
    }
}

QVariantMap PlayerController::notesForEditing()
{
    if (!notesAvailable())
        return {};
    try {
        return {{"fingerprint", m_fingerprint}, {"title", m_title}, {"text", m_library->notes(m_fingerprint)}};
    } catch (const std::exception &error) {
        report("Could not open notes: " + QString::fromUtf8(error.what()));
        return {};
    }
}

QString PlayerController::saveNotes(const QString &fingerprint, const QString &text)
{
    try {
        // The editor captures this identity when opened; never use m_fingerprint
        // here, since another book may have replaced the listening session.
        m_library->saveNotes(fingerprint, text);
        return {};
    } catch (const std::exception &error) {
        return "Could not save notes: " + QString::fromUtf8(error.what());
    }
}

void PlayerController::setEqualizerEnabled(bool enabled)
{
    if (m_equalizerEnabled == enabled)
        return;
    invalidateLoudness();
    m_equalizerEnabled = enabled;
    m_equalizerUpdate.start();
    emit equalizerChanged();
}

void PlayerController::setEqualizerTone(double tone)
{
    if (!std::isfinite(tone))
        return;
    tone = std::clamp(tone, -12.0, 12.0);
    if (m_equalizerTone == tone)
        return;
    invalidateLoudness();
    m_equalizerTone = tone;
    m_equalizerUpdate.start();
    emit equalizerChanged();
}

void PlayerController::setEqualizerFrequency(double frequency)
{
    if (!std::isfinite(frequency))
        return;
    frequency = std::clamp(frequency, 200.0, 4000.0);
    if (m_equalizerFrequency == frequency)
        return;
    invalidateLoudness();
    m_equalizerFrequency = frequency;
    m_equalizerUpdate.start();
    emit equalizerChanged();
}

void PlayerController::setEqualizerBoost(double boost)
{
    if (!std::isfinite(boost))
        return;
    boost = std::clamp(boost, 0.0, 24.0);
    if (m_equalizerBoost == boost)
        return;
    invalidateLoudness();
    m_equalizerBoost = boost;
    m_equalizerUpdate.start();
    emit equalizerChanged();
}

void PlayerController::resetEqualizer()
{
    invalidateLoudness();
    m_equalizerTone = 0;
    m_equalizerFrequency = 1000;
    m_equalizerBoost = 0;
    m_equalizerUpdate.start();
    emit equalizerChanged();
}

QVariantList PlayerController::equalizerResponse() const
{
    const auto c = TiltEqualizer::coefficients(m_equalizerEnabled ? m_equalizerTone : 0, m_equalizerFrequency, false);
    QVariantList points;
    for (int i = 0; i <= 96; ++i)
        points.append(c.decibels(20 * std::pow(1000, i / 96.0)));
    return points;
}

void PlayerController::invalidateLoudness()
{
    if (m_loudnessAnalyzing) {
        cancelLoudness();
    } else if (!m_loudnessStatus.isEmpty()) {
        m_loudnessStatus.clear();
        emit loudnessChanged();
    }
}

void PlayerController::cancelLoudness()
{
    if (!m_loudnessAnalyzing)
        return;
    m_loudnessCancel->store(true);
    ++m_loudnessGeneration;
    m_loudnessAnalyzing = false;
    m_loudnessStatus = "Measurement cancelled.";
    emit loudnessChanged();
}

void PlayerController::autoLoudness()
{
    if (!loudnessAvailable() || m_loudnessAnalyzing)
        return;
    const auto cancel = m_loudnessCancel = std::make_shared<std::atomic_bool>(false);
    const quint64 request = ++m_loudnessGeneration;
    const quint64 load = m_generation;
    const QString path = m_path;
    const double start = m_loudnessStart = chapterStart();
    const double end = m_loudnessEnd = chapterEnd();
    const double tone = m_equalizerTone, frequency = m_equalizerFrequency;
    m_loudnessAnalyzing = true;
    m_loudnessStatus = "Measuring this chapter…";
    emit loudnessChanged();
    auto *watcher = new QFutureWatcher<LoudnessResult>(this);
    connect(watcher, &QFutureWatcher<LoudnessResult>::finished, this, [this, watcher, request, load, start, end, cancel] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (request != m_loudnessGeneration || load != m_generation || cancel->load())
            return;
        m_loudnessAnalyzing = false;
        if (result.cancelled || chapterStart() != start || chapterEnd() != end) {
            m_loudnessStatus = "Measurement cancelled.";
        } else if (!result.error.isEmpty()) {
            m_loudnessStatus = "Could not measure loudness: " + result.error;
        } else {
            const double requested = std::max(0.0, -18.0-result.integrated);
            const double boost = std::min(24.0, requested);
            setEqualizerBoost(boost);
            setEqualizerEnabled(true);
            if (requested > 24)
                m_loudnessStatus = QString("Measured %1 LKFS. Boost capped at +24 dB; −18 LKFS may not be reached.").arg(result.integrated, 0, 'f', 1);
            else if (requested == 0)
                m_loudnessStatus = QString("Measured %1 LKFS. No boost needed.").arg(result.integrated, 0, 'f', 1);
            else
                m_loudnessStatus = QString("Measured %1 LKFS. Boost +%2 dB toward −18 LKFS; peaks remain limited.")
                    .arg(result.integrated, 0, 'f', 1).arg(boost, 0, 'f', 1);
        }
        emit loudnessChanged();
    });
    watcher->setFuture(QtConcurrent::run([path, start, end, tone, frequency, cancel] {
        return LoudnessReader::read(path, start, end, tone, frequency, cancel);
    }));
}

void PlayerController::shutdown()
{
    cancelLoudness();
    if (m_cancel)
        m_cancel->store(true);
    m_checkpoint.stop();
    save();
}
