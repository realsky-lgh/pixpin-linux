#pragma once

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>

class MagnifierWidget : public QWidget {
    Q_OBJECT
public:
    explicit MagnifierWidget(QWidget *parent = nullptr);

    void updatePositionAndData(const QPoint &cursorPos, const QImage &screenImage);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint m_cursorPos;
    QColor m_pixelColor;
    QImage m_zoomedImage;
    int m_zoomFactor = 5;
    int m_sampleRadius = 15; // 31x31 sampled pixels
};
