#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

// Global preferences shown in the Settings window. Every change is applied
// and saved immediately; there is no Save/Cancel step.
class AppPreferences : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppPreferences)
    QML_UNCREATABLE("The application owns its preferences")
    Q_PROPERTY(bool resumeAtLaunch READ resumeAtLaunch WRITE setResumeAtLaunch NOTIFY resumeAtLaunchChanged)
    Q_PROPERTY(bool stopAfterChapterDefault READ stopAfterChapterDefault WRITE setStopAfterChapterDefault NOTIFY stopAfterChapterDefaultChanged)
    Q_PROPERTY(bool fillCover READ fillCover WRITE setFillCover NOTIFY fillCoverChanged)
    Q_PROPERTY(int playerTextSize READ playerTextSize WRITE setPlayerTextSize NOTIFY playerTextSizeChanged)
    Q_PROPERTY(int captionSize READ captionSize WRITE setCaptionSize NOTIFY captionSizeChanged)
    Q_PROPERTY(int captionTransparency READ captionTransparency WRITE setCaptionTransparency NOTIFY captionTransparencyChanged)
    Q_PROPERTY(int minimumPlayerTextSize READ minimumPlayerTextSizeValue CONSTANT)
    Q_PROPERTY(int maximumPlayerTextSize READ maximumPlayerTextSizeValue CONSTANT)
    Q_PROPERTY(int minimumCaptionSize READ minimumCaptionSizeValue CONSTANT)
    Q_PROPERTY(int maximumCaptionSize READ maximumCaptionSizeValue CONSTANT)
public:
    static constexpr int minimumPlayerTextSize = 12, maximumPlayerTextSize = 24, defaultPlayerTextSize = 16;
    static constexpr int minimumCaptionSize = 12, maximumCaptionSize = 108, defaultCaptionSize = 26;

    explicit AppPreferences(const QString &settingsPath, QObject *parent = nullptr);
    int minimumPlayerTextSizeValue() const { return minimumPlayerTextSize; }
    int maximumPlayerTextSizeValue() const { return maximumPlayerTextSize; }
    int minimumCaptionSizeValue() const { return minimumCaptionSize; }
    int maximumCaptionSizeValue() const { return maximumCaptionSize; }
    bool resumeAtLaunch() const { return m_resumeAtLaunch; }
    bool stopAfterChapterDefault() const { return m_stopAfterChapterDefault; }
    bool fillCover() const { return m_fillCover; }
    int playerTextSize() const { return m_playerTextSize; }
    int captionSize() const { return m_captionSize; }
    int captionTransparency() const { return m_captionTransparency; }
    void setResumeAtLaunch(bool resume);
    void setStopAfterChapterDefault(bool enabled);
    void setFillCover(bool fill);
    void setPlayerTextSize(int size);
    void setCaptionSize(int size);
    void setCaptionTransparency(int percent);
signals:
    void resumeAtLaunchChanged();
    void stopAfterChapterDefaultChanged();
    void fillCoverChanged();
    void playerTextSizeChanged();
    void captionSizeChanged();
    void captionTransparencyChanged();
private:
    void store(const char *key, const QVariant &value);
    QSettings m_settings;
    bool m_resumeAtLaunch = true;
    bool m_stopAfterChapterDefault = false;
    bool m_fillCover = true;
    int m_playerTextSize = defaultPlayerTextSize;
    int m_captionSize = defaultCaptionSize;
    int m_captionTransparency = 0;
};
