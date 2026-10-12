#pragma once

#include <QObject>
#include <QSettings>
#include <QSize>
#include <QTimer>

class QWindow;

// Desktop window size/state only; placement belongs to the window manager.
class WindowPreferences : public QObject {
public:
    WindowPreferences(QWindow &window, const QString &settingsPath);
    void save();

private:
    void rememberSize();
    QWindow &m_window;
    QSettings m_settings;
    QTimer m_resize;
    QSize m_normalSize;
    bool m_maximized = false;
};
