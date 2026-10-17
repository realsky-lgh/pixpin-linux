#include "ImageUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace ImageUtils {

QImage createMosaic(const QImage &source, int blockSize) {
    if (source.isNull() || blockSize <= 1) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int width = result.width();
    int height = result.height();

    for (int y = 0; y < height; y += blockSize) {
        for (int x = 0; x < width; x += blockSize) {
            int blockW = std::min(blockSize, width - x);
            int blockH = std::min(blockSize, height - y);

            // Sample center pixel
            int sampleX = x + blockW / 2;
            int sampleY = y + blockH / 2;
            QRgb sampleColor = result.pixel(sampleX, sampleY);

            // Fill block with sampled color
            for (int by = 0; by < blockH; ++by) {
                QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y + by));
                for (int bx = 0; bx < blockW; ++bx) {
                    line[x + bx] = sampleColor;
                }
            }
        }
    }

    return result;
}

QColor getPixelColor(const QImage &image, int x, int y) {
    if (image.isNull() || x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
        return QColor(0, 0, 0);
    }
    return image.pixelColor(x, y);
}

QString colorToHex(const QColor &color) {
    return color.name(QColor::HexRgb).toUpper();
}

QPixmap addDropShadow(const QPixmap &source, int shadowRadius, const QColor &shadowColor) {
    if (source.isNull()) return source;

    int w = source.width() + shadowRadius * 2;
    int h = source.height() + shadowRadius * 2;

    QPixmap result(w, h);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw multi-layer soft shadow
    for (int i = 0; i < shadowRadius; ++i) {
        qreal factor = static_cast<qreal>(shadowRadius - i) / shadowRadius;
        int alpha = static_cast<int>(shadowColor.alpha() * (1.0 - factor * factor) * 0.4);
        QColor stepColor = shadowColor;
        stepColor.setAlpha(alpha);

        painter.setPen(Qt::NoPen);
        painter.setBrush(stepColor);
        QRectF shadowRect(shadowRadius - i, shadowRadius - i, 
                          source.width() + i * 2, source.height() + i * 2);
        painter.drawRoundedRect(shadowRect, 8, 8);
    }

    // Draw actual image in the center
    painter.drawPixmap(shadowRadius, shadowRadius, source);
    painter.end();

    return result;
}

} // namespace ImageUtils
