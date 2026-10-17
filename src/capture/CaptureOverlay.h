#pragma once

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QRect>
#include <QPoint>
#include <QScreen>
#include "MagnifierWidget.h"
#include "../ui/ToolBarWidget.h"
#include "../ui/AnnotationLayer.h"

enum class CaptureState {
    Idle,           // Waiting for user to drag selection
    Selecting,      // Currently dragging to select ROI
    Selected,       // ROI selected, toolbar shown
    Resizing,       // Resizing selection via handles
    Moving,         // Moving selection rect
    Annotating      // Drawing annotations inside selection
};

enum class ResizeHandle {
    None,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CaptureOverlay(QScreen *screen, const QPixmap &screenRawPixmap, QWidget *parent = nullptr);

    QScreen *targetScreen() const { return m_screen; }
    void clearSelection();

signals:
    void pinRequested(const QPixmap &pixmap);
    void ocrRequested(const QPixmap &pixmap);
    void selectionStarted(CaptureOverlay *sender);
    void overlayClosed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI();
    ResizeHandle hitTest(const QPoint &pos) const;
    void updateCursorForHandle(ResizeHandle handle);
    QRect getHandleRect(const QPoint &center) const;
    QPixmap getComposedPixmap(bool highRes = false) const;

    void onCopy();
    void onSave();
    void onPin();
    void onOcr();

    QScreen *m_screen = nullptr;
    QPixmap m_rawPixmap;       // High-resolution physical monitor slice
    QPixmap m_displayPixmap;   // Scaled to match monitor logical size
    QImage m_displayImage;
    QRect m_selectedRect;
    CaptureState m_state = CaptureState::Idle;
    ResizeHandle m_activeHandle = ResizeHandle::None;

    QPoint m_startPos;
    QPoint m_lastMousePos;

    MagnifierWidget *m_magnifier = nullptr;
    ToolBarWidget *m_toolbar = nullptr;
    AnnotationLayer *m_annotations = nullptr;

    const int m_handleSize = 8;
};
