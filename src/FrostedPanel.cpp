#include "FrostedPanel.h"

#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
QImage gaussianBlur(const QImage &source, double sigma)
{
    const int radius = int(std::ceil(3 * sigma));
    std::vector<double> kernel(2 * radius + 1);
    double total = 0;
    for (int i = -radius; i <= radius; ++i) {
        kernel[i + radius] = std::exp(-double(i*i) / (2*sigma*sigma));
        total += kernel[i + radius];
    }
    for (double &weight : kernel)
        weight /= total;
    QImage intermediate(source.size(), QImage::Format_ARGB32_Premultiplied);
    QImage output(source.size(), QImage::Format_ARGB32_Premultiplied);
    // Separable Gaussian, clamped at the padded image edges. Premultiplied
    // channels avoid dark fringes around transparent or missing artwork.
    for (int pass = 0; pass < 2; ++pass) {
        const QImage &input = pass == 0 ? source : intermediate;
        QImage &destination = pass == 0 ? intermediate : output;
        for (int y = 0; y < source.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(destination.scanLine(y));
            for (int x = 0; x < source.width(); ++x) {
                double r = 0, g = 0, b = 0, a = 0;
                for (int k = -radius; k <= radius; ++k) {
                    const int sx = pass == 0 ? std::clamp(x+k, 0, source.width()-1) : x;
                    const int sy = pass == 0 ? y : std::clamp(y+k, 0, source.height()-1);
                    const QRgb pixel = reinterpret_cast<const QRgb *>(input.constScanLine(sy))[sx];
                    const double weight = kernel[k+radius];
                    r += qRed(pixel)*weight; g += qGreen(pixel)*weight;
                    b += qBlue(pixel)*weight; a += qAlpha(pixel)*weight;
                }
                row[x] = qRgba(qRound(r), qRound(g), qRound(b), qRound(a));
            }
        }
    }
    return output;
}
}

FrostedPanel::FrostedPanel(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FrostedPanel::invalidate()
{
    m_dirty = true;
    update();
}

void FrostedPanel::setSource(const QUrl &source)
{
    if (m_source == source)
        return;
    m_source = source;
    m_artwork = {};
    if (source.isLocalFile()) {
        QImageReader reader(source.toLocalFile());
        const QSize size = reader.size();
        if (size.isValid())
            reader.setScaledSize(size.scaled(768, 768, Qt::KeepAspectRatio));
        m_artwork = reader.read();
    }
    invalidate();
    emit sourceChanged();
}

void FrostedPanel::setArtworkRect(const QRectF &rect)
{
    if (m_artworkRect == rect)
        return;
    m_artworkRect = rect;
    invalidate();
    emit artworkRectChanged();
}

void FrostedPanel::setTint(const QColor &tint)
{
    if (m_tint == tint)
        return;
    m_tint = tint;
    update();
    emit tintChanged();
}

void FrostedPanel::geometryChange(const QRectF &geometry, const QRectF &previous)
{
    QQuickPaintedItem::geometryChange(geometry, previous);
    if (geometry.size() != previous.size())
        invalidate();
}

void FrostedPanel::paint(QPainter *painter)
{
    if (width() <= 0 || height() <= 0)
        return;
    constexpr double padding = 36; // Three sigma outside the clipped panel.
    const QRectF padded(-padding, -padding, width()+2*padding, height()+2*padding);
    if (m_dirty) {
        m_dirty = false;
        m_blurred = {};
        if (!m_artwork.isNull() && m_artworkRect.isValid()) {
            // At most 256 pixels along the longest axis; Gaussian convolution
            // runs only after a cover/framing/size change, never on audio ticks.
            const double scale = std::min(0.25, 256.0 / std::max(padded.width(), padded.height()));
            QImage sample(QSize(qCeil(padded.width()*scale), qCeil(padded.height()*scale)),
                          QImage::Format_ARGB32_Premultiplied);
            sample.fill(Qt::transparent);
            QPainter capture(&sample);
            capture.setRenderHint(QPainter::SmoothPixmapTransform);
            capture.scale(scale, scale);
            capture.translate(padding, padding);
            capture.drawImage(m_artworkRect, m_artwork);
            capture.end();
            m_blurred = gaussianBlur(sample, std::max(0.5, 12*scale));
        }
    }
    QPainterPath clip;
    clip.addRoundedRect(boundingRect(), 20, 20);
    painter->setClipPath(clip);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (!m_blurred.isNull())
        painter->drawImage(padded, m_blurred);
    painter->fillRect(boundingRect(), m_tint);
}
