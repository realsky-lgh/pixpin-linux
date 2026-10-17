#pragma once

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include "../ui/ToolBarWidget.h"

class MagnifierWidget : public QWidget {
    Q_OBJECT
public:
    explicit MagnifierWidget(QWidget *parent = nullptr);

    void updatePositionAndData(const QPoint &cursorPos, const QImage &rawImage, double scaleX = 1.0, double scaleY = 1.0);
    void setSelectionShape(SelectionShape shape);
    SelectionShape selectionShape() const { return m_selectionShape; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint m_cursorPos;
    QColor m_pixelColor;
    QImage m_zoomedImage;
    SelectionShape m_selectionShape = SelectionShape::Rectangle;
    int m_sampleRadius = 15; // 31x31 sampled pixels
};
