#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>

class PinWindow : public QWidget {
    Q_OBJECT
public:
    explicit PinWindow(const QPixmap &pixmap, QWidget *parent = nullptr);

    void setScale(qreal factor);
    qreal scale() const { return m_scaleFactor; }

signals:
    void ocrRequested(const QPixmap &pixmap);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void copyToClipboard();
    void saveToFile();

    QPixmap m_originalPixmap;
    qreal m_scaleFactor = 1.0;
    qreal m_opacity = 1.0;
    bool m_isDragging = false;
    QPoint m_dragPosition;
    bool m_alwaysOnTop = true;
};
