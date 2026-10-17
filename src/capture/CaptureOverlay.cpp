#include "CaptureOverlay.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QInputDialog>
#include <QWindow>

CaptureOverlay::CaptureOverlay(QScreen *screen, const QPixmap &screenRawPixmap, QWidget *parent)
    : QWidget(parent, Qt::BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool)
    , m_screen(screen)
    , m_rawPixmap(screenRawPixmap) {
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    winId();
    if (windowHandle() && m_screen) {
        windowHandle()->setScreen(m_screen);
    }
    if (m_screen) {
        setGeometry(m_screen->geometry());
        m_scaleX = (double)m_rawPixmap.width() / m_screen->geometry().width();
        m_scaleY = (double)m_rawPixmap.height() / m_screen->geometry().height();
    }
    m_rawImage = m_rawPixmap.toImage();

    setupUI();
}

void CaptureOverlay::setupUI() {
    m_magnifier = new MagnifierWidget(this);
    m_magnifier->show();

    m_toolbar = new ToolBarWidget(this);
    m_toolbar->hide();

    m_annotations = new AnnotationLayer(this);

    // Toolbar connections
    connect(m_toolbar, &ToolBarWidget::toolSelected, [this](ToolType tool) {
        m_annotations->setToolType(tool);
        if (tool != ToolType::None) {
            m_state = CaptureState::Annotating;
            setCursor(Qt::CrossCursor);
        } else {
            m_state = CaptureState::Selected;
            setCursor(Qt::ArrowCursor);
        }
    });

    connect(m_toolbar, &ToolBarWidget::colorChanged, [this](const QColor &color) {
        m_annotations->setColor(color);
    });

    connect(m_toolbar, &ToolBarWidget::strokeWidthChanged, [this](int width) {
        m_annotations->setStrokeWidth(width);
    });

    connect(m_toolbar, &ToolBarWidget::undoTriggered, [this]() {
        m_annotations->undo();
        update();
    });

    connect(m_toolbar, &ToolBarWidget::copyTriggered, this, &CaptureOverlay::onCopy);
    connect(m_toolbar, &ToolBarWidget::saveTriggered, this, &CaptureOverlay::onSave);
    connect(m_toolbar, &ToolBarWidget::pinTriggered, this, &CaptureOverlay::onPin);
    connect(m_toolbar, &ToolBarWidget::ocrTriggered, this, &CaptureOverlay::onOcr);
    connect(m_toolbar, &ToolBarWidget::closeTriggered, [this]() {
        emit overlayClosed();
    });
}

void CaptureOverlay::clearSelection() {
    if (m_state != CaptureState::Idle) {
        m_state = CaptureState::Idle;
        m_selectedRect = QRect();
        m_toolbar->hide();
        m_magnifier->show();
        update();
    }
}

ResizeHandle CaptureOverlay::hitTest(const QPoint &pos) const {
    if (m_selectedRect.isNull() || !m_selectedRect.isValid()) return ResizeHandle::None;

    QRect r = m_selectedRect.normalized();

    if (getHandleRect(r.topLeft()).contains(pos)) return ResizeHandle::TopLeft;
    if (getHandleRect(QPoint(r.center().x(), r.top())).contains(pos)) return ResizeHandle::Top;
    if (getHandleRect(r.topRight()).contains(pos)) return ResizeHandle::TopRight;
    if (getHandleRect(QPoint(r.right(), r.center().y())).contains(pos)) return ResizeHandle::Right;
    if (getHandleRect(r.bottomRight()).contains(pos)) return ResizeHandle::BottomRight;
    if (getHandleRect(QPoint(r.center().x(), r.bottom())).contains(pos)) return ResizeHandle::Bottom;
    if (getHandleRect(r.bottomLeft()).contains(pos)) return ResizeHandle::BottomLeft;
    if (getHandleRect(QPoint(r.left(), r.center().y())).contains(pos)) return ResizeHandle::Left;

    return ResizeHandle::None;
}

QRect CaptureOverlay::getHandleRect(const QPoint &center) const {
    return QRect(center.x() - m_handleSize / 2, center.y() - m_handleSize / 2, m_handleSize, m_handleSize);
}

void CaptureOverlay::updateCursorForHandle(ResizeHandle handle) {
    switch (handle) {
    case ResizeHandle::TopLeft:
    case ResizeHandle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case ResizeHandle::TopRight:
    case ResizeHandle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case ResizeHandle::Top:
    case ResizeHandle::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case ResizeHandle::Left:
    case ResizeHandle::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        if (m_state == CaptureState::Selected) {
            setCursor(Qt::SizeAllCursor);
        } else if (m_state == CaptureState::Annotating) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        break;
    }
}

void CaptureOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Fast GPU-interpolated draw of raw pixmap to logical rect
    p.drawPixmap(rect(), m_rawPixmap);

    // Ultra-fast 4-rect dimming without polygon clipping overhead
    QColor dimColor(0, 0, 0, 115);
    if (m_selectedRect.isValid() && !m_selectedRect.isEmpty()) {
        QRect r = m_selectedRect.normalized();
        if (r.top() > 0)
            p.fillRect(0, 0, width(), r.top(), dimColor);
        if (r.bottom() < height() - 1)
            p.fillRect(0, r.bottom() + 1, width(), height() - (r.bottom() + 1), dimColor);
        if (r.left() > 0)
            p.fillRect(0, r.top(), r.left(), r.height(), dimColor);
        if (r.right() < width() - 1)
            p.fillRect(r.right() + 1, r.top(), width() - (r.right() + 1), r.height(), dimColor);

        // Render annotations inside selection
        p.save();
        p.setClipRect(r);
        m_annotations->render(p, m_rawImage);
        p.restore();

        // Selection Border
        p.setPen(QPen(QColor(0, 122, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);

        // Control Points (Handles)
        if (m_state == CaptureState::Selected || m_state == CaptureState::Resizing) {
            p.setPen(QPen(QColor(0, 122, 255), 1.5));
            p.setBrush(Qt::white);

            QList<QPoint> handles = {
                r.topLeft(),
                QPoint(r.center().x(), r.top()),
                r.topRight(),
                QPoint(r.right(), r.center().y()),
                r.bottomRight(),
                QPoint(r.center().x(), r.bottom()),
                r.bottomLeft(),
                QPoint(r.left(), r.center().y())
            };

            for (const QPoint &pt : handles) {
                p.drawRect(getHandleRect(pt));
            }
        }

        // Dimension badge
        QString dimText = QString("%1 × %2").arg(r.width()).arg(r.height());
        QFont font("Sans", 9, QFont::Bold);
        p.setFont(font);
        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(dimText) + 12;
        int textH = fm.height() + 6;

        int badgeY = r.top() - textH - 4;
        if (badgeY < 4) badgeY = r.top() + 4;

        QRect badgeRect(r.left(), badgeY, textW, textH);
        p.fillRect(badgeRect, QColor(20, 20, 20, 210));
        p.setPen(Qt::white);
        p.drawText(badgeRect, Qt::AlignCenter, dimText);
    } else {
        p.fillRect(rect(), dimColor);
    }
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    m_startPos = event->pos();
    m_lastMousePos = event->pos();

    if (m_state == CaptureState::Annotating) {
        if (m_annotations->toolType() == ToolType::Text) {
            bool ok;
            QString text = QInputDialog::getText(this, "添加文本标注", "请输入标注文本:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                m_annotations->addText(event->pos(), text);
                update();
            }
        } else {
            m_annotations->startDrawing(event->pos());
        }
        return;
    }

    if (m_state == CaptureState::Selected) {
        m_activeHandle = hitTest(event->pos());
        if (m_activeHandle != ResizeHandle::None) {
            m_state = CaptureState::Resizing;
            return;
        }

        if (m_selectedRect.normalized().contains(event->pos())) {
            m_state = CaptureState::Moving;
            return;
        }

        // Click outside restarts selection
        m_state = CaptureState::Selecting;
        m_selectedRect = QRect(event->pos(), QSize(0, 0));
        m_toolbar->hide();
        m_magnifier->show();
        emit selectionStarted(this);
        update();
        return;
    }

    if (m_state == CaptureState::Idle) {
        m_state = CaptureState::Selecting;
        m_selectedRect = QRect(event->pos(), QSize(0, 0));
        emit selectionStarted(this);
        update();
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    if (m_magnifier && m_magnifier->isVisible()) {
        m_magnifier->updatePositionAndData(pos, m_rawImage, m_scaleX, m_scaleY);
    }

    if (m_state == CaptureState::Annotating) {
        m_annotations->updateDrawing(pos);
        update();
        return;
    }

    if (m_state == CaptureState::Selecting) {
        m_selectedRect = QRect(m_startPos, pos).normalized();
        update();
        return;
    }

    if (m_state == CaptureState::Moving) {
        QPoint delta = pos - m_lastMousePos;
        m_selectedRect.translate(delta);
        m_lastMousePos = pos;
        m_toolbar->updatePosition(m_selectedRect, rect());
        update();
        return;
    }

    if (m_state == CaptureState::Resizing) {
        QRect r = m_selectedRect.normalized();
        switch (m_activeHandle) {
        case ResizeHandle::TopLeft: r.setTopLeft(pos); break;
        case ResizeHandle::Top: r.setTop(pos.y()); break;
        case ResizeHandle::TopRight: r.setTopRight(pos); break;
        case ResizeHandle::Right: r.setRight(pos.x()); break;
        case ResizeHandle::BottomRight: r.setBottomRight(pos); break;
        case ResizeHandle::Bottom: r.setBottom(pos.y()); break;
        case ResizeHandle::BottomLeft: r.setBottomLeft(pos); break;
        case ResizeHandle::Left: r.setLeft(pos.x()); break;
        default: break;
        }
        m_selectedRect = r.normalized();
        m_toolbar->updatePosition(m_selectedRect, rect());
        update();
        return;
    }

    if (m_state == CaptureState::Selected) {
        ResizeHandle h = hitTest(pos);
        updateCursorForHandle(h);
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    if (m_state == CaptureState::Annotating) {
        m_annotations->finishDrawing(event->pos());
        update();
        return;
    }

    if (m_state == CaptureState::Selecting || m_state == CaptureState::Resizing || m_state == CaptureState::Moving) {
        m_selectedRect = m_selectedRect.normalized();

        // Filter out accidental clicks
        if (m_selectedRect.width() > 10 && m_selectedRect.height() > 10) {
            m_state = CaptureState::Selected;
            m_magnifier->hide();
            m_toolbar->updatePosition(m_selectedRect, rect());
            m_toolbar->show();
            setCursor(Qt::ArrowCursor);
        } else {
            m_state = CaptureState::Idle;
            m_selectedRect = QRect();
            m_toolbar->hide();
            m_magnifier->show();
        }
        update();
    }
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_selectedRect.normalized().contains(event->pos())) {
        onCopy();
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit overlayClosed();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        onCopy();
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        onCopy();
    } else if (event->key() == Qt::Key_S && (event->modifiers() & Qt::ControlModifier)) {
        onSave();
    } else if (event->key() == Qt::Key_P && (event->modifiers() & Qt::ControlModifier)) {
        onPin();
    } else if (event->key() == Qt::Key_Z && (event->modifiers() & Qt::ControlModifier)) {
        m_annotations->undo();
        update();
    } else {
        QWidget::keyPressEvent(event);
    }
}

QPixmap CaptureOverlay::getComposedPixmap(bool highRes) const {
    QRect r = m_selectedRect.normalized();
    if (!r.isValid() || r.isEmpty()) return QPixmap();

    // Map logical rect r to raw physical coordinates
    QRect rawRect(
        qRound(r.x() * m_scaleX),
        qRound(r.y() * m_scaleY),
        qRound(r.width() * m_scaleX),
        qRound(r.height() * m_scaleY)
    );
    rawRect = rawRect.intersected(m_rawPixmap.rect());

    QPixmap rawCrop = m_rawPixmap.copy(rawRect);

    if (!highRes) {
        // Normal resolution for Pinning
        QPixmap result = rawCrop.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(-r.topLeft());
        const_cast<AnnotationLayer *>(m_annotations)->render(p, m_rawImage);
        p.end();
        return result;
    }

    // High resolution for OCR and saving
    QPixmap result(rawCrop.size());
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawPixmap(0, 0, rawCrop);
    p.scale(m_scaleX, m_scaleY);
    p.translate(-r.topLeft());
    const_cast<AnnotationLayer *>(m_annotations)->render(p, m_rawImage);
    p.end();

    return result;
}

void CaptureOverlay::onCopy() {
    QPixmap result = getComposedPixmap(true);
    if (!result.isNull()) {
        QApplication::clipboard()->setPixmap(result);
    }
    emit overlayClosed();
}

void CaptureOverlay::onSave() {
    QPixmap result = getComposedPixmap(true);
    if (result.isNull()) return;

    m_toolbar->hide();
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString fileName = QString("PixPin_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存截图", defaultPath + "/" + fileName, "PNG 图像 (*.png);;JPEG 图像 (*.jpg)");

    if (!filePath.isEmpty()) {
        result.save(filePath);
    }
    emit overlayClosed();
}

void CaptureOverlay::onPin() {
    QPixmap result = getComposedPixmap(false);
    if (!result.isNull()) {
        emit pinRequested(result);
    }
    emit overlayClosed();
}

void CaptureOverlay::onOcr() {
    // Send 100% native resolution image for maximum OCR accuracy
    QPixmap result = getComposedPixmap(true);
    if (!result.isNull()) {
        emit ocrRequested(result);
    }
    emit overlayClosed();
}
