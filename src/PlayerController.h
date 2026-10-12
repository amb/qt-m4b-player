#pragma once

#include "Library.h"
#include "MediaData.h"
#include "MpvEngine.h"
#include <QHash>
#include <QElapsedTimer>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class PlayerController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(PlayerController)
    QML_UNCREATABLE("The application owns the playback session")
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString author READ author NOTIFY stateChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY stateChanged)
    Q_PROPERTY(QUrl coverUrl READ coverUrl NOTIFY stateChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(double position READ position NOTIFY stateChanged)
    Q_PROPERTY(double duration READ duration NOTIFY stateChanged)
    Q_PROPERTY(double speed READ speed NOTIFY stateChanged)
    Q_PROPERTY(double volume READ volume NOTIFY stateChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY stateChanged)
    Q_PROPERTY(int currentChapter READ currentChapter NOTIFY stateChanged)
    Q_PROPERTY(double chapterStart READ chapterStart NOTIFY stateChanged)
    Q_PROPERTY(double chapterEnd READ chapterEnd NOTIFY stateChanged)
    Q_PROPERTY(QString captionText READ captionText NOTIFY stateChanged)
    Q_PROPERTY(double captionScroll READ captionScroll NOTIFY stateChanged)
    Q_PROPERTY(bool captionsAvailable READ captionsAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool captionsEnabled READ captionsEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool indexingCaptions READ indexingCaptions NOTIFY stateChanged)
    Q_PROPERTY(bool stopAfterChapter READ stopAfterChapter NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recentBooks READ recentBooks NOTIFY stateChanged)
public:
    explicit PlayerController(Library *library, const QString &cacheDirectory, bool silent = false,
                              QObject *parent = nullptr);
    ~PlayerController() override;
    QString title() const { return m_title; }
    QString author() const { return m_author; }
    QUrl fileUrl() const { return QUrl::fromLocalFile(m_path); }
    QUrl coverUrl() const { return m_cover; }
    bool loaded() const { return m_loaded; }
    bool playing() const { return m_playing; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double speed() const { return m_speed; }
    double volume() const { return m_volume; }
    QVariantList chapters() const;
    int currentChapter() const { return m_currentChapter; }
    double chapterStart() const;
    double chapterEnd() const;
    QString captionText() const;
    double captionScroll() const;
    bool captionsAvailable() const { return !m_cues.isEmpty(); }
    bool captionsEnabled() const { return m_captions; }
    bool indexingCaptions() const { return m_indexingCaptions; }
    bool stopAfterChapter() const { return m_stopAfter; }
    QString error() const { return m_error; }
    QVariantList recentBooks() const { return m_recents; }
    QString engineVersion() const { return m_engine.version(); }
    Q_INVOKABLE void openBook(const QUrl &url);
    Q_INVOKABLE void openLocalPath(const QString &path) { openBook(QUrl::fromLocalFile(path)); }
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void skip(double seconds);
    Q_INVOKABLE void navigateChapter(int offset);
    Q_INVOKABLE void selectChapter(int index);
    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setCaptions(bool enabled);
    Q_INVOKABLE void setStopAfterChapter(bool enabled);
    Q_INVOKABLE void clearError();
    void restoreLastBook();
    void shutdown();
signals:
    void stateChanged();
private:
    void beginCaptionIndex(quint64 generation, const QString &path, const Cancellation &cancel);
    void applyResume();
    void save();
    void savePreferences();
    void report(const QString &message);
    Library *m_library;
    QString m_cacheDirectory;
    MpvEngine m_engine;
    QTimer m_checkpoint;
    Cancellation m_cancel;
    quint64 m_generation = 0;
    QString m_path, m_fingerprint, m_title = "Open an audiobook", m_author, m_error;
    QUrl m_cover;
    QVector<Chapter> m_chapters;
    QVector<CaptionCue> m_cues;
    QHash<int, double> m_chapterPositions;
    QVariantList m_recents;
    double m_position = 0, m_duration = 0, m_speed = 1, m_volume = 100, m_resume = 0;
    int m_currentChapter = -1;
    bool m_loaded = false, m_playing = false, m_metadataReady = false;
    bool m_positionTouched = false, m_captionsTouched = false, m_stopTouched = false;
    bool m_captions = false, m_stopAfter = false, m_indexingCaptions = false;
    // Ignore old clock reports until a precise seek reaches its intended destination.
    std::optional<double> m_seekTarget;
    QElapsedTimer m_seekElapsed;
};
