#include "SystemTray.h"
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>

SystemTray::SystemTray(QObject *parent)
    : QSystemTrayIcon(parent) {
    setupUI();
}

void SystemTray::setupUI() {
    // Generate programmatic stylish PixPin icon
    QPixmap iconPix(32, 32);
    iconPix.fill(Qt::transparent);

    QPainter p(&iconPix);
    p.setRenderHint(QPainter::Antialiasing);

    // Blue rounded badge
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 122, 255));
    p.drawRoundedRect(2, 2, 28, 28, 7, 7);

    // White pin / target symbol
    p.setPen(QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(QPoint(16, 13), 5, 5);
    p.drawLine(16, 18, 16, 25);
    p.end();

    setIcon(QIcon(iconPix));
    setToolTip("PixPin Linux - 智能截图、贴图与离线OCR");

    m_menu = new QMenu();
    m_menu->setStyleSheet(
        "QMenu {"
        "   background-color: #242428;"
        "   color: #F0F0F0;"
        "   border: 1px solid rgba(255,255,255,0.15);"
        "   border-radius: 6px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 20px;"
        "   border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #007AFF;"
        "   color: white;"
        "}"
    );

    auto *captureAct = m_menu->addAction("📸 截取屏幕");
    auto *pinClipAct = m_menu->addAction("📌 贴剪贴板图片");
    m_menu->addSeparator();

    auto *aboutAct = m_menu->addAction("ℹ️ 关于 PixPin Linux");
    m_menu->addSeparator();

    auto *quitAct = m_menu->addAction("🚪 退出");

    connect(captureAct, &QAction::triggered, this, &SystemTray::captureRequested);
    connect(pinClipAct, &QAction::triggered, this, &SystemTray::pinClipboardRequested);
    connect(quitAct, &QAction::triggered, this, &SystemTray::quitRequested);

    connect(aboutAct, &QAction::triggered, []() {
        QMessageBox::about(nullptr, "关于 PixPin Linux",
            "<h3>PixPin Linux 1.0</h3>"
            "<p>基于 Qt6 + Tesseract OCR 原生打造的 Linux 高性能截图、贴图与文本识别工具。</p>"
            "<p>深度兼容 Ubuntu Wayland 与 X11 环境。</p>");
    });

    setContextMenu(m_menu);

    connect(this, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            emit captureRequested();
        }
    });
}
