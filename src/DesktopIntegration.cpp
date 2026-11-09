#include "DesktopIntegration.h"

#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>

void DesktopIntegration::copyText(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void DesktopIntegration::revealFile(const QString &path)
{
    const QFileInfo file(path);
    const QUrl folder = QUrl::fromLocalFile(file.absolutePath());
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QDesktopServices::openUrl(folder);
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    message << QStringList{QUrl::fromLocalFile(file.absoluteFilePath()).toString()} << QString();
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 5000));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [watcher, folder] {
        if (QDBusPendingReply<>(*watcher).isError())
            QDesktopServices::openUrl(folder);
        watcher->deleteLater();
    });
}
