#include "AnnotationLayer.h"
#include "../utils/ImageUtils.h"
#include <QPainterPath>
#include <QtMath>

void RectItem::draw(QPainter &painter, const QImage &) {
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.normalized(), 4, 4);
}

void EllipseItem::draw(QPainter &painter, const QImage &) {
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(rect.normalized());
}

void ArrowItem::draw(QPainter &painter, const QImage &) {
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);

    // Draw main line
    painter.drawLine(start, end);

    // Calculate arrow head
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();
    qreal length = qSqrt(dx * dx + dy * dy);
    if (length < 2) return;

    qreal angle = qAtan2(dy, dx);
    qreal arrowSize = qMax(12.0, width * 3.5);

    QPointF p1 = end;
    QPointF p2 = end - QPointF(arrowSize * qCos(angle - M_PI / 6), arrowSize * qSin(angle - M_PI / 6));
    QPointF p3 = end - QPointF(arrowSize * qCos(angle + M_PI / 6), arrowSize * qSin(angle + M_PI / 6));

    QPolygonF arrowHead;
    arrowHead << p1 << p2 << p3;
    painter.drawPolygon(arrowHead);
}

void PenItem::draw(QPainter &painter, const QImage &) {
    if (points.size() < 2) return;
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    path.moveTo(points[0]);
    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points[i]);
    }
    painter.drawPath(path);
}

void StepItem::draw(QPainter &painter, const QImage &) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(pos, radius, radius);

    painter.setPen(Qt::white);
    QFont font("Sans", 10, QFont::Bold);
    painter.setFont(font);
    QRect textRect(pos.x() - radius, pos.y() - radius, radius * 2, radius * 2);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(number));
}

void TextItem::draw(QPainter &painter, const QImage &) {
    painter.setPen(color);
    QFont font("Sans", fontSize, QFont::Medium);
    painter.setFont(font);
    painter.drawText(pos, text);
}

void MosaicItem::draw(QPainter &painter, const QImage &baseImage) {
    if (baseImage.isNull()) return;
    QRect normRect = rect.normalized().intersected(baseImage.rect());
    if (normRect.isEmpty()) return;

    QImage subImg = baseImage.copy(normRect);
    QImage mosaic = ImageUtils::createMosaic(subImg, blockSize);
    painter.drawImage(normRect.topLeft(), mosaic);
}

AnnotationLayer::AnnotationLayer(QObject *parent)
    : QObject(parent) {}

void AnnotationLayer::startDrawing(const QPoint &pos) {
    if (m_currentTool == ToolType::None) return;

    m_isDrawing = true;
    m_startPoint = pos;
    m_currentPoint = pos;

    if (m_currentTool == ToolType::Pen) {
        m_currentPenPoints.clear();
        m_currentPenPoints.append(pos);
    }
}

void AnnotationLayer::updateDrawing(const QPoint &pos) {
    if (!m_isDrawing) return;
    m_currentPoint = pos;

    if (m_currentTool == ToolType::Pen) {
        m_currentPenPoints.append(pos);
    }
}

void AnnotationLayer::finishDrawing(const QPoint &pos) {
    if (!m_isDrawing) return;
    m_currentPoint = pos;
    m_isDrawing = false;

    QRect r(m_startPoint, m_currentPoint);

    switch (m_currentTool) {
    case ToolType::Rect: {
        auto item = std::make_shared<RectItem>();
        item->rect = r;
        item->color = m_currentColor;
        item->width = m_strokeWidth;
        m_items.append(item);
        break;
    }
    case ToolType::Ellipse: {
        auto item = std::make_shared<EllipseItem>();
        item->rect = r;
        item->color = m_currentColor;
        item->width = m_strokeWidth;
        m_items.append(item);
        break;
    }
    case ToolType::Arrow: {
        auto item = std::make_shared<ArrowItem>();
        item->start = m_startPoint;
        item->end = m_currentPoint;
        item->color = m_currentColor;
        item->width = m_strokeWidth;
        m_items.append(item);
        break;
    }
    case ToolType::Pen: {
        if (m_currentPenPoints.size() > 1) {
            auto item = std::make_shared<PenItem>();
            item->points = m_currentPenPoints;
            item->color = m_currentColor;
            item->width = m_strokeWidth;
            m_items.append(item);
        }
        m_currentPenPoints.clear();
        break;
    }
    case ToolType::Step: {
        auto item = std::make_shared<StepItem>();
        item->pos = pos;
        item->number = m_stepCounter++;
        item->color = m_currentColor;
        m_items.append(item);
        break;
    }
    case ToolType::Mosaic: {
        auto item = std::make_shared<MosaicItem>();
        item->rect = r;
        m_items.append(item);
        break;
    }
    default:
        break;
    }
}

void AnnotationLayer::addText(const QPoint &pos, const QString &text) {
    if (text.isEmpty()) return;
    auto item = std::make_shared<TextItem>();
    item->pos = pos;
    item->text = text;
    item->color = m_currentColor;
    m_items.append(item);
}

void AnnotationLayer::undo() {
    if (!m_items.isEmpty()) {
        m_items.removeLast();
    }
}

void AnnotationLayer::clear() {
    m_items.clear();
    m_currentPenPoints.clear();
    m_stepCounter = 1;
    m_isDrawing = false;
}

void AnnotationLayer::render(QPainter &painter, const QImage &baseImage) {
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw completed items
    for (const auto &item : m_items) {
        item->draw(painter, baseImage);
    }

    // Draw active drawing preview
    if (m_isDrawing) {
        QRect r(m_startPoint, m_currentPoint);
        switch (m_currentTool) {
        case ToolType::Rect: {
            RectItem preview;
            preview.rect = r;
            preview.color = m_currentColor;
            preview.width = m_strokeWidth;
            preview.draw(painter, baseImage);
            break;
        }
        case ToolType::Ellipse: {
            EllipseItem preview;
            preview.rect = r;
            preview.color = m_currentColor;
            preview.width = m_strokeWidth;
            preview.draw(painter, baseImage);
            break;
        }
        case ToolType::Arrow: {
            ArrowItem preview;
            preview.start = m_startPoint;
            preview.end = m_currentPoint;
            preview.color = m_currentColor;
            preview.width = m_strokeWidth;
            preview.draw(painter, baseImage);
            break;
        }
        case ToolType::Pen: {
            PenItem preview;
            preview.points = m_currentPenPoints;
            preview.color = m_currentColor;
            preview.width = m_strokeWidth;
            preview.draw(painter, baseImage);
            break;
        }
        case ToolType::Mosaic: {
            MosaicItem preview;
            preview.rect = r;
            preview.draw(painter, baseImage);
            break;
        }
        default:
            break;
        }
    }
}
