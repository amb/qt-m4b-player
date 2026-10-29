#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// CPU-rendered artwork backdrop, shared by software and accelerated Qt Quick.
class FrostedPanel : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QRectF artworkRect READ artworkRect WRITE setArtworkRect NOTIFY artworkRectChanged)
    Q_PROPERTY(QColor tint READ tint WRITE setTint NOTIFY tintChanged)
public:
    explicit FrostedPanel(QQuickItem *parent = nullptr);
    QUrl source() const { return m_source; }
    QRectF artworkRect() const { return m_artworkRect; }
    QColor tint() const { return m_tint; }
    void setSource(const QUrl &source);
    void setArtworkRect(const QRectF &rect);
    void setTint(const QColor &tint);
    void paint(QPainter *painter) override;
signals:
    void sourceChanged();
    void artworkRectChanged();
    void tintChanged();
protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
private:
    void invalidate();
    QUrl m_source;
    QRectF m_artworkRect;
    QColor m_tint;
    QImage m_artwork, m_blurred;
    bool m_dirty = true;
};
