#include "MagnifierWidget.h"
#include "../utils/ImageUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>

MagnifierWidget::MagnifierWidget(QWidget *parent)
    : QWidget(parent) {
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(160, 200);
}

void MagnifierWidget::updatePositionAndData(const QPoint &cursorPos, const QImage &screenImage) {
    m_cursorPos = cursorPos;

    if (!screenImage.isNull()) {
        m_pixelColor = ImageUtils::getPixelColor(screenImage, cursorPos.x(), cursorPos.y());

        // Crop sample region around cursor
        int sampleSize = m_sampleRadius * 2 + 1;
        int sx = cursorPos.x() - m_sampleRadius;
        int sy = cursorPos.y() - m_sampleRadius;

        QImage sample(sampleSize, sampleSize, QImage::Format_ARGB32_Premultiplied);
        sample.fill(Qt::black);

        QPainter sp(&sample);
        sp.drawImage(0, 0, screenImage, sx, sy, sampleSize, sampleSize);
        sp.end();

        m_zoomedImage = sample.scaled(140, 110, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    // Position the magnifier slightly offset from the cursor
    int offsetX = 20;
    int offsetY = 20;
    int targetX = cursorPos.x() + offsetX;
    int targetY = cursorPos.y() + offsetY;

    if (parentWidget()) {
        if (targetX + width() > parentWidget()->width()) {
            targetX = cursorPos.x() - width() - offsetX;
        }
        if (targetY + height() > parentWidget()->height()) {
            targetY = cursorPos.y() - height() - offsetY;
        }
    }

    move(targetX, targetY);
    update();
}

void MagnifierWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Rounded card background
    QRectF cardRect(0, 0, width(), height());
    QPainterPath path;
    path.addRoundedRect(cardRect, 10, 10);

    p.fillPath(path, QColor(25, 25, 28, 230));
    p.setPen(QPen(QColor(80, 80, 85, 200), 1.5));
    p.drawPath(path);

    // Draw Zoomed Screen Area
    QRect zoomRect(10, 10, 140, 110);
    p.setClipRect(zoomRect);
    p.drawImage(zoomRect, m_zoomedImage);

    // Crosshair in center
    p.setPen(QPen(QColor(0, 255, 255, 200), 1.5));
    int centerX = zoomRect.center().x();
    int centerY = zoomRect.center().y();
    p.drawLine(centerX - 8, centerY, centerX + 8, centerY);
    p.drawLine(centerX, centerY - 8, centerX, centerY + 8);

    p.setClipping(false);
    p.setPen(QPen(QColor(120, 120, 130), 1));
    p.drawRect(zoomRect);

    // Information area
    QFont font("Sans", 9);
    p.setFont(font);

    // Coordinate
    p.setPen(QColor(220, 220, 220));
    QString posStr = QString("坐标: (%1, %2)").arg(m_cursorPos.x()).arg(m_cursorPos.y());
    p.drawText(10, 140, posStr);

    // Color Swatch
    QRect swatchRect(10, 150, 16, 16);
    p.fillRect(swatchRect, m_pixelColor);
    p.setPen(Qt::white);
    p.drawRect(swatchRect);

    // HEX & RGB
    p.setPen(QColor(255, 255, 255));
    QString hexStr = ImageUtils::colorToHex(m_pixelColor);
    QString rgbStr = QString("RGB: %1,%2,%3").arg(m_pixelColor.red()).arg(m_pixelColor.green()).arg(m_pixelColor.blue());
    p.drawText(32, 163, hexStr);

    p.setPen(QColor(180, 180, 180));
    QFont smallFont("Sans", 8);
    p.setFont(smallFont);
    p.drawText(10, 185, rgbStr);
}
