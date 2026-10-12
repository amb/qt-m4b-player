#include "WindowPreferences.h"
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScreen>
#include <QTemporaryDir>
#include <QWindow>
#include <QtTest>
#include <algorithm>

class WindowTests : public QObject {
    Q_OBJECT
    static void configure(QWindow &window)
    {
        window.setMinimumSize({390, 600});
        window.resize(520, 800);
    }
private slots:
    void initTestCase() { qGuiApp->setQuitOnLastWindowClosed(false); }

    void normalSizeAndTemporaryStates()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("settings.ini");
        {
            QWindow window;
            configure(window);
            WindowPreferences preferences(window, path);
            window.show();
            window.resize(640, 700);
            QTest::qWait(20);
            preferences.save();
        }
        {
            QWindow window;
            configure(window);
            WindowPreferences preferences(window, path);
            QCOMPARE(window.size(), QSize(640, 700));
            QCOMPARE(window.windowState(), Qt::WindowNoState);
            window.show();
            // Deliver resize before state change to exercise transition ordering.
            window.resize(800, 800);
            window.setWindowState(Qt::WindowMaximized);
            QTest::qWait(20);
            preferences.save();
            window.setWindowState(Qt::WindowFullScreen);
            window.resize(900, 900);
            QTest::qWait(20);
            preferences.save();
            window.setWindowState(Qt::WindowMinimized);
            preferences.save();
        }
        QWindow window;
        configure(window);
        WindowPreferences preferences(window, path);
        QCOMPARE(window.size(), QSize(640, 700));
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        window.setWindowState(Qt::WindowNoState);
        window.resize(615, 680);
        // Saving immediately must include a final resize before the timer fires.
        preferences.save();
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("window/width").toInt(), 615);
        QCOMPARE(settings.value("window/height").toInt(), 680);
        QVERIFY(!settings.value("window/maximized").toBool());
    }

    void sizeFitsScreenAndMinimum()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("settings.ini");
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue("window/width", 100000);
        settings.setValue("window/height", 100000);
        settings.sync();
        QWindow window;
        configure(window);
        {
            WindowPreferences preferences(window, path);
            const QSize available = window.screen()->availableGeometry().size();
            QCOMPARE(window.size(), QSize(std::max(390, available.width()), std::max(600, available.height())));
        }
        settings.setValue("window/width", -1);
        settings.setValue("window/height", 0);
        settings.sync();
        WindowPreferences preferences(window, path);
        QCOMPARE(window.size(), QSize(390, 600));
    }

    void applicationRestoresBeforeShowingAndSavesOnExit()
    {
        QTemporaryDir directory;
        const QString settingsPath = directory.filePath("settings.ini");
        QSettings settings(settingsPath, QSettings::IniFormat);
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("QT_QUICK_BACKEND", "software");
        environment.insert("QT_SCALE_FACTOR", "1");
        for (bool maximized : {false, true}) {
            settings.setValue("window/width", 640);
            settings.setValue("window/height", 700);
            settings.setValue("window/maximized", maximized);
            settings.sync();
            QProcess process;
            process.setProcessEnvironment(environment);
            const QString screenshot = directory.filePath("window.png");
            process.start(QCoreApplication::applicationDirPath() + "/qt-m4b-player",
                {"--no-resume", "--smoke-test", "--data-dir", directory.path(), "--screenshot", screenshot});
            QVERIFY(process.waitForStarted());
            QVERIFY(process.waitForFinished(10000));
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
            if (!maximized)
                QCOMPARE(QImage(screenshot).size(), QSize(640, 700));
            settings.sync();
            QCOMPARE(settings.value("window/width").toInt(), 640);
            QCOMPARE(settings.value("window/height").toInt(), 700);
            QCOMPARE(settings.value("window/maximized").toBool(), maximized);
        }
    }
};
QTEST_MAIN(WindowTests)
#include "WindowTests.moc"
