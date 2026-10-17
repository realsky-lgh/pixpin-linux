#pragma once

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QRect>
#include <QPoint>
#include <QScreen>
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
    void drawMagnifier(QPainter &p, const QPoint &cursorPos);

    void onCopy();
    void onSave();
    void onPin();
    void onOcr();

    QScreen *m_screen = nullptr;
    QPixmap m_rawPixmap;       // High-resolution physical monitor slice
    QPixmap m_scaledPixmap;    // Screen logical size pixmap for instant 1:1 blit
    QPixmap m_dimmedPixmap;    // Pre-dimmed background for zero-overhead repainting
    QImage m_rawImage;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;

    QRect m_selectedRect;
    CaptureState m_state = CaptureState::Idle;
    ResizeHandle m_activeHandle = ResizeHandle::None;

    QPoint m_startPos;
    QPoint m_lastMousePos;
    QPoint m_currentCursorPos;

    ToolBarWidget *m_toolbar = nullptr;
    AnnotationLayer *m_annotations = nullptr;

    const int m_handleSize = 8;
};
