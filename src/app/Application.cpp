#include "Application.h"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QScreen>
#include <QDebug>

Application::Application(QObject *parent)
    : QObject(parent)
    , m_capturer(std::make_unique<ScreenCapturer>())
    , m_ocrEngine(std::make_unique<OcrEngine>()) {

    connect(m_capturer.get(), &ScreenCapturer::screenCaptured, this, &Application::onScreenCaptured);
    connect(m_capturer.get(), &ScreenCapturer::captureFailed, this, &Application::onCaptureFailed);
}

Application::~Application() {
    closeAllOverlays();
}

void Application::start() {
    m_tray = new SystemTray(this);
    connect(m_tray, &SystemTray::captureRequested, this, &Application::triggerCapture);
    connect(m_tray, &SystemTray::pinClipboardRequested, this, &Application::pinFromClipboard);
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);

    m_tray->show();
}

void Application::triggerCapture() {
    closeAllOverlays();
    m_capturer->capture();
}

void Application::closeAllOverlays() {
    for (auto *o : m_activeOverlays) {
        disconnect(o, nullptr, nullptr, nullptr);
        o->close();
    }
    m_activeOverlays.clear();
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

    closeAllOverlays();

    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        onCaptureFailed("未检测到有效显示器");
        return;
    }

    QRect totalGeometry;
    for (QScreen *s : screens) {
        totalGeometry = totalGeometry.united(s->geometry());
    }

    double ratioX = (double)pixmap.width() / totalGeometry.width();
    double ratioY = (double)pixmap.height() / totalGeometry.height();

    for (QScreen *s : screens) {
        QRect screenGeo = s->geometry();
        QRect rawRect(
            qRound((screenGeo.x() - totalGeometry.x()) * ratioX),
            qRound((screenGeo.y() - totalGeometry.y()) * ratioY),
            qRound(screenGeo.width() * ratioX),
            qRound(screenGeo.height() * ratioY)
        );
        rawRect = rawRect.intersected(pixmap.rect());
        QPixmap screenRawPixmap = pixmap.copy(rawRect);

        auto *overlay = new CaptureOverlay(s, screenRawPixmap);
        m_activeOverlays.append(overlay);

        connect(overlay, &CaptureOverlay::pinRequested, this, &Application::onPinRequested);
        connect(overlay, &CaptureOverlay::ocrRequested, this, &Application::onOcrRequested);

        // When user starts selecting on one monitor, clear selection on other monitors
        connect(overlay, &CaptureOverlay::selectionStarted, this, [this](CaptureOverlay *active) {
            for (auto *o : m_activeOverlays) {
                if (o != active) {
                    o->clearSelection();
                }
            }
        });

        // When any overlay finishes/closes, close all overlays
        connect(overlay, &CaptureOverlay::overlayClosed, this, &Application::closeAllOverlays);

        overlay->showFullScreen();
    }
}

void Application::onCaptureFailed(const QString &reason) {
    qWarning() << "Capture failed:" << reason;
    if (m_tray) {
        m_tray->showMessage("PixPin 截图失败", reason, QSystemTrayIcon::Warning, 3000);
    }
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
