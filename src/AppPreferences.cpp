#include "AppPreferences.h"

#include <algorithm>

namespace {
int boundedInt(const QSettings &settings, const char *key, int fallback, int low, int high)
{
    bool valid = false;
    const int value = settings.value(key, fallback).toInt(&valid);
    return valid ? std::clamp(value, low, high) : fallback;
}
}

AppPreferences::AppPreferences(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat)
{
    m_resumeAtLaunch = m_settings.value("playback/resumeAtLaunch", true).toBool();
    m_stopAfterChapterDefault = m_settings.value("playback/stopAfterChapterDefault", false).toBool();
    m_fillCover = m_settings.value("appearance/fillCover", true).toBool();
    m_playerTextSize = boundedInt(m_settings, "appearance/playerTextSize", defaultPlayerTextSize,
                                  minimumPlayerTextSize, maximumPlayerTextSize);
    m_captionSize = boundedInt(m_settings, "appearance/captionSize", defaultCaptionSize,
                               minimumCaptionSize, maximumCaptionSize);
    m_captionTransparency = boundedInt(m_settings, "appearance/captionTransparency", 0, 0, 100);
}

void AppPreferences::store(const char *key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
}

void AppPreferences::setResumeAtLaunch(bool resume)
{
    if (resume == m_resumeAtLaunch)
        return;
    m_resumeAtLaunch = resume;
    store("playback/resumeAtLaunch", resume);
    emit resumeAtLaunchChanged();
}

void AppPreferences::setStopAfterChapterDefault(bool enabled)
{
    if (enabled == m_stopAfterChapterDefault)
        return;
    m_stopAfterChapterDefault = enabled;
    store("playback/stopAfterChapterDefault", enabled);
    emit stopAfterChapterDefaultChanged();
}

void AppPreferences::setFillCover(bool fill)
{
    if (fill == m_fillCover)
        return;
    m_fillCover = fill;
    store("appearance/fillCover", fill);
    emit fillCoverChanged();
}

void AppPreferences::setPlayerTextSize(int size)
{
    size = std::clamp(size, minimumPlayerTextSize, maximumPlayerTextSize);
    if (size == m_playerTextSize)
        return;
    m_playerTextSize = size;
    store("appearance/playerTextSize", size);
    emit playerTextSizeChanged();
}

void AppPreferences::setCaptionSize(int size)
{
    size = std::clamp(size, minimumCaptionSize, maximumCaptionSize);
    if (size == m_captionSize)
        return;
    m_captionSize = size;
    store("appearance/captionSize", size);
    emit captionSizeChanged();
}

void AppPreferences::setCaptionTransparency(int percent)
{
    percent = std::clamp(percent, 0, 100);
    if (percent == m_captionTransparency)
        return;
    m_captionTransparency = percent;
    store("appearance/captionTransparency", percent);
    emit captionTransparencyChanged();
}
