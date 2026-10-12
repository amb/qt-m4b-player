#include "AppPreferences.h"

#include <algorithm>

AppPreferences::AppPreferences(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat)
{
    bool valid = false;
    const int size = m_settings.value("appearance/captionSize", 26).toInt(&valid);
    m_captionSize = valid ? std::clamp(size, 18, 42) : 26;
    m_fillCover = m_settings.value("appearance/fillCover", true).toBool();
}

void AppPreferences::setCaptionSize(int size)
{
    size = std::clamp(size, 18, 42);
    if (size == m_captionSize)
        return;
    m_captionSize = size;
    m_settings.setValue("appearance/captionSize", size);
    m_settings.sync();
    emit captionSizeChanged();
}

void AppPreferences::setFillCover(bool fill)
{
    if (fill == m_fillCover)
        return;
    m_fillCover = fill;
    m_settings.setValue("appearance/fillCover", fill);
    m_settings.sync();
    emit fillCoverChanged();
}
