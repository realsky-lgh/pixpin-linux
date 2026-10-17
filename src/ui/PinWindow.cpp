#include "PinWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <algorithm>

PinWindow::PinWindow(const QPixmap &pixmap, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_originalPixmap(pixmap) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    resize(m_originalPixmap.size());
}

void PinWindow::setScale(qreal factor) {
    m_scaleFactor = std::clamp(factor, 0.2, 5.0);
    QSize newSize = (QSizeF(m_originalPixmap.size()) * m_scaleFactor).toSize();
    resize(newSize);
    update();
}

void PinWindow::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setOpacity(m_opacity);

    // Draw scaled pixmap
    painter.drawPixmap(rect(), m_originalPixmap);

    // Draw subtle border around pinned window
    painter.setOpacity(0.4);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void PinWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void PinWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    }
}

void PinWindow::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Adjust Opacity with Ctrl + Wheel
        qreal delta = event->angleDelta().y() > 0 ? 0.05 : -0.05;
        m_opacity = std::clamp(m_opacity + delta, 0.2, 1.0);
        update();
    } else {
        // Zoom with Wheel
        qreal delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
        setScale(m_scaleFactor + delta);
    }
    event->accept();
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        close();
    }
}

void PinWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        copyToClipboard();
    } else if (event->key() == Qt::Key_S && (event->modifiers() & Qt::ControlModifier)) {
        saveToFile();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: #242428;"
        "   color: #F0F0F0;"
        "   border: 1px solid rgba(255,255,255,0.15);"
        "   border-radius: 6px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px;"
        "   border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #007AFF;"
        "   color: white;"
        "}"
    );

    auto *copyAct = menu.addAction("📋 复制图片 (Ctrl+C)");
    auto *saveAct = menu.addAction("💾 保存图片... (Ctrl+S)");
    auto *ocrAct = menu.addAction("🔤 提取文字 (OCR)");
    menu.addSeparator();

    auto *zoom100Act = menu.addAction("🔍 缩放: 100%");
    auto *zoom50Act = menu.addAction("🔍 缩放: 50%");
    auto *zoom200Act = menu.addAction("🔍 缩放: 200%");
    menu.addSeparator();

    auto *flipHAct = menu.addAction("↔️ 水平翻转");
    auto *flipVAct = menu.addAction("↕️ 垂直翻转");
    menu.addSeparator();

    auto *topAct = menu.addAction(m_alwaysOnTop ? "📌 取消置顶" : "📌 始终置顶");
    auto *closeAct = menu.addAction("✕ 关闭贴图 (双击 / Esc)");

    connect(copyAct, &QAction::triggered, this, &PinWindow::copyToClipboard);
    connect(saveAct, &QAction::triggered, this, &PinWindow::saveToFile);
    connect(ocrAct, &QAction::triggered, [this]() {
        emit ocrRequested(m_originalPixmap);
    });

    connect(zoom100Act, &QAction::triggered, [this]() { setScale(1.0); });
    connect(zoom50Act, &QAction::triggered, [this]() { setScale(0.5); });
    connect(zoom200Act, &QAction::triggered, [this]() { setScale(2.0); });

    connect(flipHAct, &QAction::triggered, [this]() {
        m_originalPixmap = m_originalPixmap.transformed(QTransform().scale(-1, 1));
        update();
    });
    connect(flipVAct, &QAction::triggered, [this]() {
        m_originalPixmap = m_originalPixmap.transformed(QTransform().scale(1, -1));
        update();
    });

    connect(topAct, &QAction::triggered, [this]() {
        m_alwaysOnTop = !m_alwaysOnTop;
        setWindowFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
        show();
    });
    connect(closeAct, &QAction::triggered, this, &QWidget::close);

    menu.exec(event->globalPos());
}

void PinWindow::copyToClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(m_originalPixmap);
}

void PinWindow::saveToFile() {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString fileName = QString("PixPin_Pin_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存贴图", defaultPath + "/" + fileName, "PNG 图像 (*.png);;JPEG 图像 (*.jpg)");

    if (!filePath.isEmpty()) {
        m_originalPixmap.save(filePath);
    }
}
