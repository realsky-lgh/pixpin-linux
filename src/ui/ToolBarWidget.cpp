#include "ToolBarWidget.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>

ToolBarWidget::ToolBarWidget(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setupUI();
}

void ToolBarWidget::setupUI() {
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 2);
    m_rootLayout->setSpacing(4);

    m_mainBar = new QWidget(this);
    m_mainBar->setObjectName("mainBar");
    m_mainBar->setFixedHeight(36);
    m_mainBar->setStyleSheet(
        "#mainBar {"
        "   background-color: rgba(30, 30, 34, 0.95);"
        "   border: 1px solid rgba(255, 255, 255, 0.15);"
        "   border-radius: 6px;"
        "}"
        "QPushButton {"
        "   background: transparent;"
        "   color: #ECECEC;"
        "   font-size: 13px;"
        "   font-weight: bold;"
        "   border: none;"
        "   border-radius: 4px;"
        "   min-width: 28px;"
        "   min-height: 28px;"
        "   max-width: 32px;"
        "   max-height: 32px;"
        "   padding: 2px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 0.15);"
        "}"
        "QPushButton:checked {"
        "   background-color: #007AFF;"
        "   color: white;"
        "}"
    );

    auto *mainLayout = new QHBoxLayout(m_mainBar);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(false);

    // Shape toggle button: Rectangle vs Ellipse/Circle
    m_btnShape = createToolButton("⭕", "切换形状: 椭圆/圆 (快捷键: Tab)");
    connect(m_btnShape, &QPushButton::clicked, [this]() {
        m_currentShape = (m_currentShape == SelectionShape::Rectangle) ? SelectionShape::Ellipse : SelectionShape::Rectangle;
        updateShapeButton();
        emit selectionShapeChanged(m_currentShape);
    });

    // Primary Action Tools
    m_btnPin = createToolButton("📌", "贴图 (Ctrl+P / 钉在屏幕上)");
    m_btnOcr = createToolButton("🔤", "提取文字 (Shift+C / 离线 OCR)");

    // Separator line
    auto *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: rgba(255,255,255,0.2);");

    // Shape/Annotation Tools
    m_btnRect = createToolButton("⬜", "矩形", true);
    m_btnEllipse = createToolButton("⭕", "椭圆", true);
    m_btnArrow = createToolButton("➔", "箭头", true);
    m_btnPen = createToolButton("✏️", "涂鸦画笔", true);
    m_btnStep = createToolButton("🔢", "序号标记 (1, 2, 3...)", true);
    m_btnMosaic = createToolButton("▒", "马赛克/模糊", true);
    m_btnText = createToolButton("T", "文本标注", true);

    m_toolGroup->addButton(m_btnRect);
    m_toolGroup->addButton(m_btnEllipse);
    m_toolGroup->addButton(m_btnArrow);
    m_toolGroup->addButton(m_btnPen);
    m_toolGroup->addButton(m_btnStep);
    m_toolGroup->addButton(m_btnMosaic);
    m_toolGroup->addButton(m_btnText);

    auto *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: rgba(255,255,255,0.2);");

    m_btnUndo = createToolButton("↩️", "撤销 (Ctrl+Z)");
    m_btnCopy = createToolButton("📋", "复制到剪贴板 (Enter / Ctrl+C)");
    m_btnSave = createToolButton("💾", "保存为图片 (Ctrl+S)");
    m_btnClose = createToolButton("✕", "关闭截图 (Esc)");

    // Assemble main layout
    mainLayout->addWidget(m_btnShape);
    mainLayout->addWidget(m_btnPin);
    mainLayout->addWidget(m_btnOcr);
    mainLayout->addWidget(sep1);
    mainLayout->addWidget(m_btnRect);
    mainLayout->addWidget(m_btnEllipse);
    mainLayout->addWidget(m_btnArrow);
    mainLayout->addWidget(m_btnPen);
    mainLayout->addWidget(m_btnStep);
    mainLayout->addWidget(m_btnMosaic);
    mainLayout->addWidget(m_btnText);
    mainLayout->addWidget(sep2);
    mainLayout->addWidget(m_btnUndo);
    mainLayout->addWidget(m_btnCopy);
    mainLayout->addWidget(m_btnSave);
    mainLayout->addWidget(m_btnClose);

    m_rootLayout->addWidget(m_mainBar);

    // Setup Sub Bar (Color & Stroke Width)
    setupSubBar();

    // Signal connections
    connect(m_btnPin, &QPushButton::clicked, this, &ToolBarWidget::pinTriggered);
    connect(m_btnOcr, &QPushButton::clicked, this, &ToolBarWidget::ocrTriggered);
    connect(m_btnUndo, &QPushButton::clicked, this, &ToolBarWidget::undoTriggered);
    connect(m_btnCopy, &QPushButton::clicked, this, &ToolBarWidget::copyTriggered);
    connect(m_btnSave, &QPushButton::clicked, this, &ToolBarWidget::saveTriggered);
    connect(m_btnClose, &QPushButton::clicked, this, &ToolBarWidget::closeTriggered);

    auto handleToolChecked = [this](QPushButton *btn, ToolType type) {
        if (btn->isChecked()) {
            for (auto *b : m_toolGroup->buttons()) {
                if (b != btn) b->setChecked(false);
            }
            m_subBar->show();
            emit toolSelected(type);
        } else {
            m_subBar->hide();
            emit toolSelected(ToolType::None);
        }
        adjustSize();
        if (m_lastTargetRect.isValid() && m_lastBoundaryRect.isValid()) {
            updatePosition(m_lastTargetRect, m_lastBoundaryRect);
        }
    };

    connect(m_btnRect, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnRect, ToolType::Rect); });
    connect(m_btnEllipse, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnEllipse, ToolType::Ellipse); });
    connect(m_btnArrow, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnArrow, ToolType::Arrow); });
    connect(m_btnPen, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnPen, ToolType::Pen); });
    connect(m_btnStep, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnStep, ToolType::Step); });
    connect(m_btnMosaic, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnMosaic, ToolType::Mosaic); });
    connect(m_btnText, &QPushButton::toggled, [=](bool) { handleToolChecked(m_btnText, ToolType::Text); });

    adjustSize();
}

class ColorSwatchButton : public QPushButton {
public:
    ColorSwatchButton(const QColor &color, QWidget *parent = nullptr)
        : QPushButton(parent), m_color(color) {
        setFixedSize(22, 22);
        setCursor(Qt::PointingHandCursor);
        setCheckable(true);
    }

    QColor color() const { return m_color; }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Safe margin: 16x16 circle centered inside 22x22 widget area
        QRectF circleRect(3.0, 3.0, 16.0, 16.0);

        // Fill circle
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(circleRect);

        // Border around circle
        if (m_color == Qt::white) {
            p.setPen(QPen(QColor(160, 160, 160), 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(circleRect);
        } else {
            p.setPen(QPen(QColor(255, 255, 255, 80), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(circleRect);
        }

        // Selected state: draw crisp outer ring
        if (isChecked()) {
            p.setPen(QPen(QColor(0, 122, 255), 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(1.0, 1.0, 20.0, 20.0));
        }
    }

private:
    QColor m_color;
};

void ToolBarWidget::setupSubBar() {
    m_subBar = new QWidget(this);
    m_subBar->setObjectName("subBar");
    m_subBar->setFixedHeight(34);
    m_subBar->setStyleSheet(
        "#subBar {"
        "   background-color: rgba(30, 30, 34, 0.95);"
        "   border: 1px solid rgba(255, 255, 255, 0.15);"
        "   border-radius: 6px;"
        "}"
    );

    auto *subLayout = new QHBoxLayout(m_subBar);
    subLayout->setContentsMargins(8, 4, 8, 4);
    subLayout->setSpacing(6);

    // Color Swatches
    QList<QColor> colors = {
        QColor(255, 59, 48),   // Red (PixPin signature)
        QColor(255, 204, 0),  // Yellow
        QColor(52, 199, 89),   // Green
        QColor(0, 122, 255),   // Blue
        QColor(255, 255, 255), // White
        QColor(28, 28, 30)     // Black
    };

    auto *colorGroup = new QButtonGroup(this);
    colorGroup->setExclusive(true);

    for (int i = 0; i < colors.size(); ++i) {
        const auto &color = colors[i];
        auto *btn = new ColorSwatchButton(color, m_subBar);
        if (i == 0) btn->setChecked(true); // Default to red
        colorGroup->addButton(btn);
        connect(btn, &QPushButton::clicked, [this, color]() {
            emit colorChanged(color);
        });
        subLayout->addWidget(btn);
    }

    auto *sep = new QFrame(m_subBar);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: rgba(255,255,255,0.2);");
    subLayout->addWidget(sep);

    // Stroke width buttons
    auto *widthGroup = new QButtonGroup(this);
    widthGroup->setExclusive(true);

    QList<QPair<QString, int>> widths = {{"细", 2}, {"中", 4}, {"粗", 6}};
    for (int i = 0; i < widths.size(); ++i) {
        const auto &pair = widths[i];
        auto *btn = new QPushButton(pair.first, m_subBar);
        btn->setFixedSize(28, 22);
        btn->setCheckable(true);
        if (pair.second == 4) btn->setChecked(true); // Default to medium
        widthGroup->addButton(btn);
        btn->setStyleSheet(
            "QPushButton {"
            "   background: rgba(255,255,255,0.1);"
            "   color: #ECECEC;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "   border-radius: 4px;"
            "   border: none;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.25); }"
            "QPushButton:checked { background: #007AFF; color: white; }"
        );
        connect(btn, &QPushButton::clicked, [this, w = pair.second]() {
            emit strokeWidthChanged(w);
        });
        subLayout->addWidget(btn);
    }

    m_rootLayout->addWidget(m_subBar, 0, Qt::AlignRight);
    m_subBar->hide(); // Hidden by default
}

QPushButton *ToolBarWidget::createToolButton(const QString &text, const QString &tooltip, bool checkable) {
    auto *btn = new QPushButton(text, m_mainBar);
    btn->setToolTip(tooltip);
    btn->setCheckable(checkable);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void ToolBarWidget::resetTools() {
    for (auto *b : m_toolGroup->buttons()) {
        b->setChecked(false);
    }
    m_subBar->hide();
    adjustSize();
    if (m_lastTargetRect.isValid() && m_lastBoundaryRect.isValid()) {
        updatePosition(m_lastTargetRect, m_lastBoundaryRect);
    }
    emit toolSelected(ToolType::None);
}

void ToolBarWidget::updatePosition(const QRect &targetRect, const QRect &boundaryRect) {
    if (targetRect.isValid()) {
        m_lastTargetRect = targetRect;
    }
    if (boundaryRect.isValid()) {
        m_lastBoundaryRect = boundaryRect;
    }

    adjustSize();
    int w = width();
    int h = height();

    // Align to the right bottom of the selection
    int x = targetRect.right() - w;
    int y = targetRect.bottom() + 6;

    // Check bottom boundary: if it extends past the bottom of the screen, place above
    if (y + h > boundaryRect.bottom()) {
        y = targetRect.top() - h - 6;
    }

    // Check top boundary: if placing above extends past the top of the screen
    if (y < boundaryRect.top()) {
        // If neither above nor below fits outside, place inside selection at bottom
        if (targetRect.bottom() - h - 6 >= boundaryRect.top()) {
            y = targetRect.bottom() - h - 6;
        } else {
            y = boundaryRect.bottom() - h - 4;
        }
    }

    // Hard boundary clamps to ensure complete visibility
    if (y + h > boundaryRect.bottom()) {
        y = boundaryRect.bottom() - h - 4;
    }
    if (y < boundaryRect.top()) {
        y = boundaryRect.top() + 4;
    }

    // Check horizontal boundaries
    if (x + w > boundaryRect.right()) {
        x = boundaryRect.right() - w - 6;
    }
    if (x < boundaryRect.left()) {
        x = boundaryRect.left() + 6;
    }

    move(x, y);
}

void ToolBarWidget::setSelectionShape(SelectionShape shape) {
    if (m_currentShape != shape) {
        m_currentShape = shape;
        updateShapeButton();
    }
}

void ToolBarWidget::updateShapeButton() {
    if (!m_btnShape) return;
    if (m_currentShape == SelectionShape::Rectangle) {
        m_btnShape->setText("⭕");
        m_btnShape->setToolTip("切换形状: 椭圆/圆 (快捷键: Tab)");
    } else {
        m_btnShape->setText("🔲");
        m_btnShape->setToolTip("切换形状: 矩形 (快捷键: Tab)");
    }
}
