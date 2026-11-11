#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QString>
#include <QVariantMap>
#include <QDBusInterface>
#include <memory>

class ScreenCapturer : public QObject {
    Q_OBJECT
public:
    explicit ScreenCapturer(QObject *parent = nullptr);
    ~ScreenCapturer() override;

    // Trigger full screen capture
    void capture();

    // Determine whether Wayland is currently running
    static bool isWayland();

signals:
    void screenCaptured(const QImage &image);
    void captureFailed(const QString &reason);

public slots:
    void onPortalResponse(uint response, const QVariantMap &results);
    void onMutterPipeWireStreamAdded(uint nodeId);

private:
    void captureX11();
    void captureWayland();
    bool captureMutterPipeWire();

    std::unique_ptr<QDBusInterface> m_portal;
    bool m_dbusConnected = false;
    bool m_isCapturing = false;
    QString m_requestPath;
    uint m_mutterNodeId = 0;
};
