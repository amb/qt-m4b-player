#include "AppPreferences.h"
#include "FrostedPanel.h"
#include "PlayerController.h"
#include "TransportIconProvider.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>
#include <memory>

class FeatureUiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        qmlRegisterUncreatableType<PlayerController>("AudiobookPlayer", 1, 0, "PlayerController", "Owned by test");
        qmlRegisterUncreatableType<AppPreferences>("AudiobookPlayer", 1, 0, "AppPreferences", "Owned by test");
        qmlRegisterType<FrostedPanel>("AudiobookPlayer", 1, 0, "FrostedPanel");
    }
    void notesKeyboardCancelSaveAndFailureKeepDraft()
    {
        QTemporaryDir directory;
        Library library(directory.filePath("library.sqlite"));
        PlayerController player(&library, directory.filePath("covers"), true);
        AppPreferences preferences(directory.filePath("settings.ini"));
        player.openLocalPath(QString(SOURCE_DIR) + "/tests/fixtures/captions.m4b");
        QTRY_VERIFY(player.notesAvailable());
        const QString identity = player.notesForEditing()["fingerprint"].toString();
        QQmlEngine engine;
        engine.addImageProvider("transport", new TransportIconProvider);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(component.createWithInitialProperties(
            {{"player", QVariant::fromValue(&player)}, {"preferences", QVariant::fromValue(&preferences)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window);
        window->show(); window->requestActivate();
        QTest::qWait(100);
        for (int width : {390, 520, 900}) {
            window->setWidth(width);
            QTest::qWait(30);
            auto *panel = root->findChild<QQuickItem *>("transportPanel");
            QVERIFY(panel);
            for (const char *name : {"primaryTransport", "secondaryTransport"}) {
                auto *row = root->findChild<QQuickItem *>(QString::fromLatin1(name));
                QVERIFY(row);
                const QRectF rect = row->mapRectToItem(panel, row->boundingRect());
                QVERIFY2(rect.left() >= 15 && rect.right() <= panel->width()-15, name);
            }
        }
        window->setWidth(520);
        QTest::qWait(50);
        auto *ratingButton = root->findChild<QQuickItem *>("ratingButton");
        auto *popup = root->findChild<QObject *>("ratingPopup");
        auto *slider = root->findChild<QQuickItem *>("ratingSlider");
        auto *openNotes = root->findChild<QQuickItem *>("openNotesButton");
        QVERIFY(ratingButton && popup && slider && openNotes);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          ratingButton->mapToScene(ratingButton->boundingRect().center()).toPoint());
        QTRY_VERIFY(popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(player.rating(), 5);
        QCOMPARE(player.position(), 0.0);
        QVERIFY(!player.playing());
        QTest::keyClick(window, Qt::Key_End);
        QTRY_COMPARE(player.rating(), 50);
        QTest::keyClick(window, Qt::Key_Home);
        QTRY_COMPARE(player.rating(), 0);
        for (int i = 0; i < 7; ++i)
            QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(player.rating(), 35);
        const QString snapshots = qEnvironmentVariable("QT_M4B_UI_SNAPSHOTS");
        if (!snapshots.isEmpty()) {
            QVERIFY(QDir().mkpath(snapshots));
            QTest::qWait(50);
            QVERIFY(window->grabWindow().save(snapshots + "/rating-and-notes.png"));
        }
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          openNotes->mapToScene(openNotes->boundingRect().center()).toPoint());
        auto *editor = root->findChild<QQuickWindow *>("notesEditor");
        QVERIFY(editor);
        QTRY_VERIFY(editor->isVisible());
        QTRY_VERIFY(!popup->property("visible").toBool());
        auto *text = editor->findChild<QQuickItem *>("notesText");
        QVERIFY(text);
        text->setProperty("text", "A draft");
        text->setProperty("cursorPosition", 7);
        text->forceActiveFocus();
        editor->requestActivate();
        QTest::qWait(50);
        QTest::keyClick(editor, Qt::Key_Space);
        QCOMPARE(text->property("text").toString(), "A draft ");
        QVERIFY(!player.playing());
        QTest::keyClick(editor, Qt::Key_Escape);
        QTRY_VERIFY(!editor->isVisible());
        QVERIFY(library.notes(identity).isEmpty());
        window->requestActivate();
        QTest::qWait(50);
        QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
        QTRY_VERIFY(editor->isVisible());
        QVERIFY(text->property("text").toString().isEmpty());
        text->setProperty("text", "Saved plain text <b>and Unicode café</b>");
        if (!snapshots.isEmpty()) {
            QVERIFY(QDir().mkpath(snapshots));
            QTest::qWait(50);
            QVERIFY(editor->grabWindow().save(snapshots + "/notes.png"));
        }
        text->forceActiveFocus(); editor->requestActivate();
        QTest::qWait(50);
        QTest::keyClick(editor, Qt::Key_S, Qt::ControlModifier);
        QTRY_VERIFY(!editor->isVisible());
        QCOMPARE(library.notes(identity), QString::fromUtf8("Saved plain text <b>and Unicode café</b>"));
        QVERIFY(!player.playing());

        window->requestActivate(); QTest::qWait(50);
        QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
        QTRY_VERIFY(editor->isVisible());
        editor->setProperty("bookFingerprint", "missing");
        text->setProperty("text", "Keep this failed draft");
        QVERIFY(QMetaObject::invokeMethod(editor, "saveDocument"));
        QVERIFY(editor->isVisible());
        QVERIFY(!editor->property("saveError").toString().isEmpty());
        QCOMPARE(text->property("text").toString(), "Keep this failed draft");
        editor->close();

        window->requestActivate(); QTest::qWait(50);
        QTest::keyClick(window, Qt::Key_E, Qt::ControlModifier);
        auto *equalizer = root->findChild<QQuickWindow *>("equalizerWindow");
        QVERIFY(equalizer);
        QTRY_VERIFY(equalizer->isVisible());
        auto *footer = equalizer->findChild<QQuickItem *>("equalizerFooter");
        QVERIFY(footer);
        auto *boost = equalizer->findChild<QQuickItem *>("boostKnob");
        QVERIFY(boost);
        for (const QSize size : {QSize(480, 620), QSize(390, 600)}) {
            equalizer->resize(size);
            QTest::qWait(50);
            const QRectF bounds = footer->mapRectToItem(equalizer->contentItem(), footer->boundingRect());
            QVERIFY(bounds.bottom() <= equalizer->height()-19);
            QVERIFY(bounds.left() >= 19 && bounds.right() <= equalizer->width()-19);
            const QRectF boostBounds = boost->mapRectToItem(equalizer->contentItem(), boost->boundingRect());
            QVERIFY(boostBounds.left() >= 19 && boostBounds.right() <= equalizer->width()-19);
        }
        equalizer->resize(480, 620);
        player.setEqualizerTone(8);
        player.setEqualizerBoost(12);
        player.setEqualizerEnabled(true);
        QTest::qWait(100);
        QVERIFY(!equalizer->grabWindow().isNull());
        if (!snapshots.isEmpty())
            QVERIFY(equalizer->grabWindow().save(snapshots + "/equalizer.png"));
        equalizer->close();
        window->requestActivate(); QTest::qWait(50);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          ratingButton->mapToScene(ratingButton->boundingRect().center()).toPoint());
        QTRY_VERIFY(popup->property("visible").toBool());
        player.openLocalPath(QString(SOURCE_DIR) + "/tests/fixtures/no-captions.m4b");
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTRY_VERIFY(player.notesAvailable());
        QCOMPARE(player.rating(), 0);
        QCOMPARE(library.find(identity).rating, 35);
        window->close();
    }
    void artworkBlurRoundsCornersAndTracksFraming()
    {
        QTemporaryDir directory;
        QImage artwork(120, 80, QImage::Format_RGB32);
        artwork.fill(Qt::black);
        for (int y = 0; y < 80; ++y)
            for (int x = 60; x < 120; ++x)
                artwork.setPixelColor(x, y, Qt::white);
        const QString file = directory.filePath("synthetic-artwork.png");
        QVERIFY(artwork.save(file));
        QQuickWindow window;
        window.setColor(Qt::red);
        window.resize(120, 80);
        auto *panel = new FrostedPanel(window.contentItem());
        panel->setSize(QSizeF(120, 80));
        panel->setSource(QUrl::fromLocalFile(file));
        panel->setArtworkRect(QRectF(0, 0, 120, 80));
        panel->setTint(Qt::transparent);
        window.show();
        QImage screenshot;
        QTRY_VERIFY(!(screenshot = window.grabWindow()).isNull());
        QCOMPARE(screenshot.pixelColor(0, 0), QColor(Qt::red));
        const int boundary = screenshot.pixelColor(60, 40).red();
        QVERIFY(boundary > 70 && boundary < 190);
        QVERIFY(screenshot.pixelColor(35, 40).red() < 30);
        QVERIFY(screenshot.pixelColor(85, 40).red() > 220);
        panel->setArtworkRect(QRectF(30, 0, 120, 80));
        QTRY_VERIFY(window.grabWindow().pixelColor(60, 40).red() < 30);
        panel->setTint(QColor(0, 0, 255, 255));
        QTRY_COMPARE(window.grabWindow().pixelColor(60, 40), QColor(Qt::blue));
    }
};
QTEST_MAIN(FeatureUiTests)
#include "FeatureUiTests.moc"
