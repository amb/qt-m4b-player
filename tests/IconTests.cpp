#include "TransportIconProvider.h"

#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

class IconTests : public QObject {
    Q_OBJECT
private slots:
    void paletteChangesRecolourRenderedIcons()
    {
        QQuickView view;
        view.engine()->addImageProvider("transport", new TransportIconProvider);
        view.setSource(QUrl::fromLocalFile(QString(FIXTURE_DIR) + "/transport-icons.qml"));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.show();
        auto *root = view.rootObject();
        QVERIFY(root);

        const auto pixelAt = [&view](int x, int y) {
            const QImage screenshot = view.grabWindow();
            return screenshot.pixelColor(qRound(x * double(screenshot.width()) / view.width()),
                                         qRound(y * double(screenshot.height()) / view.height()));
        };
        // Exercise the real Button -> palette -> icon URL -> renderer path.
        // Changing colours in the same view also catches stale cached tints.
        for (const QColor &colour : {QColor(Qt::white), QColor(Qt::black), QColor("#f07820")}) {
            root->setProperty("color", colour == Qt::white ? QColor(Qt::black) : QColor(Qt::white));
            root->setProperty("foreground", colour);
            QTRY_COMPARE(pixelAt(48, 48), colour);
            // Transparent parts of the SVG must leave the background visible.
            QCOMPARE(pixelAt(34, 34), root->property("color").value<QColor>());
        }

        root->setProperty("iconChecked", true);
        root->setProperty("foreground", QColor("#40e0b0"));
        QTRY_COMPARE(pixelAt(48, 48), QColor("#40e0b0"));
        QCOMPARE(pixelAt(34, 34), QColor("#306090"));

        root->setProperty("iconChecked", false);
        root->setProperty("color", QColor(Qt::black));
        root->setProperty("foreground", QColor(255, 0, 0, 128));
        QTRY_COMPARE(pixelAt(48, 48), QColor(128, 0, 0));
    }
};
QTEST_MAIN(IconTests)
#include "IconTests.moc"
