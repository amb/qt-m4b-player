#pragma once

#include <QQuickImageProvider>

// Rasterize and tint the vendored SVGs without requiring scene graph effects.
class TransportIconProvider : public QQuickImageProvider {
public:
    TransportIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
