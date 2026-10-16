#include "Library.h"
#include "PlayerController.h"
#include "WindowPreferences.h"
#include "AppPreferences.h"
#include "TransportIconProvider.h"
#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <clocale>
#include <iostream>

namespace {
// QQuickStyle has no public "is this style installed?" query, so probe the QML
// import roots directly. The KDE runtime installs styles under its import path.
bool qmlStyleAvailable(const QString &modulePath)
{
    QStringList roots{QLibraryInfo::path(QLibraryInfo::QmlImportsPath)};
    const QByteArray extra = qgetenv("QML2_IMPORT_PATH");
    if (!extra.isEmpty()) {
        const QList<QByteArray> paths = extra.split(':');
        for (const QByteArray &path : paths)
            roots.append(QString::fromLocal8Bit(path));
    }
    for (const QString &root : roots) {
        if (!root.isEmpty() && QFileInfo::exists(root + '/' + modulePath + "/qmldir"))
            return true;
    }
    return false;
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Qt adopts the system locale; libmpv requires C numeric parsing. Set this
    // before creating the engine or starting workers, retaining other categories.
    std::setlocale(LC_NUMERIC, "C");
    QCoreApplication::setOrganizationName("qt-m4b-player");
    QCoreApplication::setApplicationName("qt-m4b-player");
    QCoreApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.amb.QtM4bPlayer"));
    // Do not pin Qt's Basic style: it ignores the platform palette. Respect an
    // explicit QT_QUICK_CONTROLS_STYLE override, otherwise adopt the KDE Quick
    // Controls style when the platform provides one so the UI follows the
    // system colour scheme. Non-KDE hosts keep Qt's default style.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        if (qmlStyleAvailable(QStringLiteral("org/kde/breeze")))
            QQuickStyle::setStyle(QStringLiteral("org.kde.breeze"));
        else if (qmlStyleAvailable(QStringLiteral("org/kde/desktop")))
            QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
    QCommandLineParser parser;
    parser.setApplicationDescription("Local M4B audiobook player — Linux prototype");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("book", "Local audiobook file to open.", "[book]");
    parser.addOption({"data-dir", "Use an isolated library/cache/settings directory.", "directory"});
    parser.addOption({"no-resume", "Start with an empty player."});
    parser.addOption({"smoke-test", "Load the UI, then exit after one second (silent audio)."});
    parser.addOption({"screenshot", "Save a window image during --smoke-test.", "path"});
    parser.addOption({"dependency-versions", "Print runtime library versions and exit."});
    parser.process(app);
    try {
        const QString data = parser.isSet("data-dir") ? parser.value("data-dir")
            : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        Library library(data + "/library.sqlite");
        PlayerController player(&library, data + "/covers", parser.isSet("smoke-test"));
        if (parser.isSet("dependency-versions")) {
            std::cout << "Qt " << qVersion() << '\n'
                      << player.engineVersion().toStdString() << '\n'
                      << "SQLite " << library.sqliteVersion().toStdString() << '\n';
            return 0;
        }
        AppPreferences preferences(data + "/settings.ini");
        QQmlApplicationEngine engine;
        engine.addImageProvider("transport", new TransportIconProvider);
        engine.setInitialProperties({{"player", QVariant::fromValue(&player)},
                                     {"preferences", QVariant::fromValue(&preferences)}});
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                         [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
        engine.loadFromModule("AudiobookPlayer", "Main");
        if (engine.rootObjects().isEmpty())
            return 1;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return 1;
        WindowPreferences windowPreferences(*window, data + "/settings.ini");
        if (window->windowState() == Qt::WindowMaximized)
            window->showMaximized();
        else
            window->showNormal();
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &player, &PlayerController::shutdown);
        const auto books = parser.positionalArguments();
        if (!books.isEmpty())
            player.openBook(QUrl::fromLocalFile(QFileInfo(books.first()).absoluteFilePath()));
        else if (!parser.isSet("no-resume"))
            player.restoreLastBook();
        if (parser.isSet("smoke-test")) {
            const QString screenshot = parser.value("screenshot");
            QTimer::singleShot(1000, &app, [window, screenshot] {
                if (!screenshot.isEmpty() && (!window || !window->grabWindow().save(screenshot))) {
                    std::cerr << "Could not save screenshot\n";
                    QCoreApplication::exit(1);
                    return;
                }
                QCoreApplication::quit();
            });
        }
        return app.exec();
    } catch (const std::exception &error) {
        std::cerr << "qt-m4b-player: " << error.what() << '\n';
        return 1;
    }
}
