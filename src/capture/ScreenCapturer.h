#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QVariantMap>

class ScreenCapturer : public QObject {
    Q_OBJECT
public:
    explicit ScreenCapturer(QObject *parent = nullptr);
    ~ScreenCapturer() override = default;

    // Trigger full screen capture
    void capture();

    // Determine whether Wayland is currently running
    static bool isWayland();

signals:
    void screenCaptured(const QPixmap &pixmap);
    void captureFailed(const QString &reason);

public slots:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    void captureX11();
    void captureWayland();

    QString m_requestPath;
};
