#include "Application.h"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QDebug>

Application::Application(QObject *parent)
    : QObject(parent)
    , m_capturer(std::make_unique<ScreenCapturer>())
    , m_ocrEngine(std::make_unique<OcrEngine>()) {

    connect(m_capturer.get(), &ScreenCapturer::screenCaptured, this, &Application::onScreenCaptured);
    connect(m_capturer.get(), &ScreenCapturer::captureFailed, this, &Application::onCaptureFailed);
}

void Application::start() {
    m_tray = new SystemTray(this);
    connect(m_tray, &SystemTray::captureRequested, this, &Application::triggerCapture);
    connect(m_tray, &SystemTray::pinClipboardRequested, this, &Application::pinFromClipboard);
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);

    m_tray->show();
}

void Application::triggerCapture() {
    if (m_activeOverlay) {
        m_activeOverlay->close();
        m_activeOverlay = nullptr;
    }
    m_capturer->capture();
}

void Application::pinFromClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    QPixmap pixmap = clipboard->pixmap();
    if (pixmap.isNull()) {
        QMessageBox::information(nullptr, "PixPin 提示", "剪贴板中没有可用的图像数据。");
        return;
    }
    onPinRequested(pixmap);
}

void Application::onScreenCaptured(const QPixmap &pixmap) {
    if (pixmap.isNull()) {
        onCaptureFailed("捕获到的屏幕图像为空");
        return;
    }

    m_activeOverlay = new CaptureOverlay(pixmap);
    connect(m_activeOverlay, &CaptureOverlay::pinRequested, this, &Application::onPinRequested);
    connect(m_activeOverlay, &CaptureOverlay::ocrRequested, this, &Application::onOcrRequested);
    connect(m_activeOverlay, &CaptureOverlay::destroyed, [this]() {
        m_activeOverlay = nullptr;
    });

    m_activeOverlay->showFullScreen();
}

void Application::onCaptureFailed(const QString &reason) {
    qWarning() << "Capture failed:" << reason;
    m_tray->showMessage("PixPin 截图失败", reason, QSystemTrayIcon::Warning, 3000);
}

void Application::onPinRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    auto *pinWin = new PinWindow(pixmap);
    connect(pinWin, &PinWindow::ocrRequested, this, &Application::onOcrRequested);
    m_pinWindows.append(pinWin);

    connect(pinWin, &PinWindow::destroyed, [this, pinWin]() {
        m_pinWindows.removeAll(pinWin);
    });

    pinWin->show();
}

void Application::onOcrRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    QString recognized = m_ocrEngine->recognizeText(pixmap);
    if (recognized.isEmpty()) {
        recognized = "(未识别出清晰文本)";
    }

    auto *dialog = new OcrResultDialog(recognized);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
