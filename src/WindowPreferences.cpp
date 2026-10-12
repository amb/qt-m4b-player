#include "WindowPreferences.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <algorithm>

WindowPreferences::WindowPreferences(QWindow &window, const QString &settingsPath)
    : m_window(window), m_settings(settingsPath, QSettings::IniFormat)
{
    const auto *screen = window.screen();
    const QSize available = screen ? screen->availableGeometry().size() : window.size();
    const int minWidth = std::max(1, window.minimumWidth());
    const int minHeight = std::max(1, window.minimumHeight());
    m_normalSize = QSize(
        std::clamp(m_settings.value("window/width", window.width()).toInt(),
                   minWidth, std::max(minWidth, available.width())),
        std::clamp(m_settings.value("window/height", window.height()).toInt(),
                   minHeight, std::max(minHeight, available.height())));
    m_maximized = m_settings.value("window/maximized", false).toBool();
    window.resize(m_normalSize);
    window.setWindowState(m_maximized ? Qt::WindowMaximized : Qt::WindowNoState);

    // A maximize/full-screen transition may deliver resize and state signals
    // together. Inspect the settled state before remembering the normal size.
    m_resize.setSingleShot(true);
    m_resize.setInterval(0);
    connect(&window, &QWindow::widthChanged, this, [this] { m_resize.start(); });
    connect(&window, &QWindow::heightChanged, this, [this] { m_resize.start(); });
    connect(&m_resize, &QTimer::timeout, this, &WindowPreferences::rememberSize);
    connect(&window, &QWindow::windowStateChanged, this, [this](Qt::WindowState state) {
        if (state == Qt::WindowMaximized)
            m_maximized = true;
        else if (state == Qt::WindowNoState)
            m_maximized = false;
        // Minimized/full-screen are temporary states, not next-launch choices.
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &WindowPreferences::save);
}

void WindowPreferences::rememberSize()
{
    if (m_window.windowState() == Qt::WindowNoState)
        m_normalSize = m_window.size();
}

void WindowPreferences::save()
{
    m_resize.stop();
    rememberSize();
    m_settings.setValue("window/width", m_normalSize.width());
    m_settings.setValue("window/height", m_normalSize.height());
    m_settings.setValue("window/maximized", m_maximized);
    m_settings.sync();
}
