#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include "../capture/ScreenCapturer.h"
#include "../capture/CaptureOverlay.h"
#include "../ui/PinWindow.h"
#include "../ocr/OcrEngine.h"
#include "../ocr/OcrResultDialog.h"
#include "SystemTray.h"

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override = default;

    void start();
    void triggerCapture();
    void pinFromClipboard();

private slots:
    void onScreenCaptured(const QPixmap &pixmap);
    void onCaptureFailed(const QString &reason);
    void onPinRequested(const QPixmap &pixmap);
    void onOcrRequested(const QPixmap &pixmap);

private:
    std::unique_ptr<ScreenCapturer> m_capturer;
    std::unique_ptr<OcrEngine> m_ocrEngine;
    SystemTray *m_tray = nullptr;
    CaptureOverlay *m_activeOverlay = nullptr;
    QList<PinWindow *> m_pinWindows;
};
