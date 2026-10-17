#pragma once

#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QRect>

namespace ImageUtils {

// Generate mosaic image for a specific region
QImage createMosaic(const QImage &source, int blockSize = 10);

// Get color at coordinates safely
QColor getPixelColor(const QImage &image, int x, int y);

// Convert color to standard hex format "#RRGGBB"
QString colorToHex(const QColor &color);

// Add modern drop shadow around pixmap
QPixmap addDropShadow(const QPixmap &source, int shadowRadius = 12, const QColor &shadowColor = QColor(0, 0, 0, 120));

} // namespace ImageUtils
