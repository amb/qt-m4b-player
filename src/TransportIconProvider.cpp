#include "TransportIconProvider.h"

#include <QColor>
#include <QImageReader>
#include <QPainter>
#include <QUrl>

TransportIconProvider::TransportIconProvider() : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage TransportIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // Colour is part of the URL so QML's image cache follows palette changes.
    const auto parts = id.split('/');
    if (parts.size() != 2)
        return {};
    const QColor tint(QUrl::fromPercentEncoding(parts[1].toUtf8()));
    if (!tint.isValid())
        return {};

    QImageReader reader(":/icons/" + parts[0] + "-light.svg");
    if (size)
        *size = reader.size();
    if (requestedSize.width() > 0 && requestedSize.height() > 0)
        reader.setScaledSize(requestedSize);
    QImage image = reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), tint);
    painter.end();
    return image;
}
