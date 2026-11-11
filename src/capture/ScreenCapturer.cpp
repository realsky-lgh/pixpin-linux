#include "ScreenCapturer.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QUrl>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QUuid>
#include <QWidget>
#include <QElapsedTimer>
#include <QThreadPool>
#include <QEventLoop>

#ifdef HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#endif

static QElapsedTimer s_portalTimer;

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent) {
    if (isWayland()) {
        m_portal = std::make_unique<QDBusInterface>(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            QDBusConnection::sessionBus()
        );

        m_dbusConnected = QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            QString(), // Match any request path
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onPortalResponse(uint, QVariantMap))
        );
        if (!m_dbusConnected) {
            qWarning() << "Failed to connect to XDG Desktop Portal Response signal";
        }
    }
}

ScreenCapturer::~ScreenCapturer() {
    if (m_dbusConnected) {
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.portal.Desktop",
            QString(),
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onPortalResponse(uint, QVariantMap))
        );
    }
}

bool ScreenCapturer::isWayland() {
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    if (sessionType == "wayland") return true;
    if (!qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) return true;
    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) return true;
    return false;
}

void ScreenCapturer::capture() {
    if (m_isCapturing) {
        return; // Debounce rapid key presses
    }
    m_isCapturing = true;

    if (isWayland()) {
        captureWayland();
    } else {
        captureX11();
    }
}

void ScreenCapturer::captureX11() {
    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        m_isCapturing = false;
        emit captureFailed("未检测到显示屏");
        return;
    }

    QRect totalGeometry;
    for (QScreen *s : screens) {
        totalGeometry = totalGeometry.united(s->geometry());
    }

    QImage virtualScreen(totalGeometry.size(), QImage::Format_ARGB32_Premultiplied);
    virtualScreen.fill(Qt::black);
    QPainter painter(&virtualScreen);

    for (QScreen *s : screens) {
        QPixmap screenPix = s->grabWindow(0);
        painter.drawPixmap(s->geometry().topLeft() - totalGeometry.topLeft(), screenPix);
    }
    painter.end();

    m_isCapturing = false;
    if (virtualScreen.isNull()) {
        emit captureFailed("X11 屏幕截取失败");
    } else {
        emit screenCaptured(virtualScreen);
    }
}

void ScreenCapturer::onMutterPipeWireStreamAdded(uint nodeId) {
    m_mutterNodeId = nodeId;
}

bool ScreenCapturer::captureMutterPipeWire() {
#ifdef HAVE_PIPEWIRE
    QElapsedTimer mutterTimer;
    mutterTimer.start();

    QDBusInterface mutter(
        "org.gnome.Mutter.ScreenCast",
        "/org/gnome/Mutter/ScreenCast",
        "org.gnome.Mutter.ScreenCast",
        QDBusConnection::sessionBus()
    );

    if (!mutter.isValid()) {
        return false;
    }

    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return false;
    }

    QRect totalGeometry;
    for (QScreen *s : screens) {
        totalGeometry = totalGeometry.united(s->geometry());
    }

    QDBusReply<QDBusObjectPath> sessionReply = mutter.call("CreateSession", QVariantMap());
    if (!sessionReply.isValid()) {
        return false;
    }
    QString sessionPath = sessionReply.value().path();

    QDBusInterface session(
        "org.gnome.Mutter.ScreenCast",
        sessionPath,
        "org.gnome.Mutter.ScreenCast.Session",
        QDBusConnection::sessionBus()
    );

    QVariantMap recordProps;
    recordProps["cursor-mode"] = (uint)0; // hidden, we draw our own cursor overlay

    QDBusReply<QDBusObjectPath> streamReply = session.call(
        "RecordArea",
        totalGeometry.x(),
        totalGeometry.y(),
        totalGeometry.width(),
        totalGeometry.height(),
        recordProps
    );

    if (!streamReply.isValid()) {
        session.call("Stop");
        return false;
    }
    QString streamPath = streamReply.value().path();

    m_mutterNodeId = 0;
    bool connected = QDBusConnection::sessionBus().connect(
        "org.gnome.Mutter.ScreenCast",
        streamPath,
        "org.gnome.Mutter.ScreenCast.Stream",
        "PipeWireStreamAdded",
        this,
        SLOT(onMutterPipeWireStreamAdded(uint))
    );

    if (!connected) {
        session.call("Stop");
        return false;
    }

    session.call("Start");

    // Wait for PipeWireStreamAdded signal (typically 2-4ms)
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (m_mutterNodeId == 0 && waitTimer.elapsed() < 300) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }

    QDBusConnection::sessionBus().disconnect(
        "org.gnome.Mutter.ScreenCast",
        streamPath,
        "org.gnome.Mutter.ScreenCast.Stream",
        "PipeWireStreamAdded",
        this,
        SLOT(onMutterPipeWireStreamAdded(uint))
    );

    if (m_mutterNodeId == 0) {
        qWarning() << "Mutter PipeWireStreamAdded timed out";
        session.call("Stop");
        return false;
    }

    uint nodeId = m_mutterNodeId;

    // Connect to PipeWire stream
    pw_init(nullptr, nullptr);
    struct pw_main_loop *pwLoop = pw_main_loop_new(nullptr);
    if (!pwLoop) {
        session.call("Stop");
        return false;
    }

    struct pw_context *pwCtx = pw_context_new(pw_main_loop_get_loop(pwLoop), nullptr, 0);
    struct pw_core *pwCore = pw_context_connect(pwCtx, nullptr, 0);

    struct GrabContext {
        int frameWidth = 0;
        int frameHeight = 0;
        QImage image;
        struct pw_stream *stream = nullptr;
    } ctx;

    static const struct pw_stream_events events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = [](void *data, uint32_t id, const struct spa_pod *param) {
            if (!param || id != SPA_PARAM_Format) return;
            auto *ctx = (GrabContext*)data;
            struct spa_video_info_raw info;
            if (spa_format_video_raw_parse(param, &info) < 0) return;
            ctx->frameWidth = info.size.width;
            ctx->frameHeight = info.size.height;
        },
        .process = [](void *userdata) {
            auto *ctx = (GrabContext*)userdata;
            struct pw_buffer *b = pw_stream_dequeue_buffer(ctx->stream);
            if (!b) return;

            struct spa_buffer *buf = b->buffer;
            if (buf->datas[0].data != NULL) {
                int stride = buf->datas[0].chunk->stride;
                if (stride <= 0) stride = ctx->frameWidth * 4;

                ctx->image = QImage(
                    (const uchar*)buf->datas[0].data,
                    ctx->frameWidth,
                    ctx->frameHeight,
                    stride,
                    QImage::Format_RGB32
                ).copy();
            }
            pw_stream_queue_buffer(ctx->stream, b);
        }
    };

    struct spa_hook streamListener;
    ctx.stream = pw_stream_new(pwCore, "pixpin-direct", nullptr);
    pw_stream_add_listener(ctx.stream, &streamListener, &events, &ctx);

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
    params[0] = (const struct spa_pod*)spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,   SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype,SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format,SPA_POD_CHOICE_ENUM_Id(4,
                                    SPA_VIDEO_FORMAT_BGRx,
                                    SPA_VIDEO_FORMAT_BGRA,
                                    SPA_VIDEO_FORMAT_RGBx,
                                    SPA_VIDEO_FORMAT_RGBA)
    );

    pw_stream_connect(
        ctx.stream,
        PW_DIRECTION_INPUT,
        nodeId,
        (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1
    );

    struct pw_loop *l = pw_main_loop_get_loop(pwLoop);
    QElapsedTimer iterateTimer;
    iterateTimer.start();
    while (ctx.image.isNull() && iterateTimer.elapsed() < 500) {
        pw_loop_iterate(l, 10);
    }

    pw_stream_destroy(ctx.stream);
    pw_main_loop_destroy(pwLoop);
    session.call("Stop");

    if (ctx.image.isNull()) {
        qWarning() << "PipeWire frame capture timed out or failed";
        return false;
    }

    qDebug() << "[PERF] Mutter+PipeWire frame grabbed in" << mutterTimer.elapsed() << "ms!";
    m_isCapturing = false;
    emit screenCaptured(ctx.image);
    return true;
#else
    return false;
#endif
}

void ScreenCapturer::captureWayland() {
    // 1. Ultra-fast path: Direct Mutter + PipeWire capture (GNOME Wayland, zero latency, no flash, no PNG write)
    if (captureMutterPipeWire()) {
        return;
    }

    // 2. Standard Wayland path: XDG Desktop Portal (fallback for KDE/other Wayland or if PipeWire fails)
    if (!m_portal || !m_portal->isValid()) {
        qWarning() << "XDG Desktop Portal not found, falling back to X11 grabWindow";
        m_isCapturing = false;
        captureX11();
        return;
    }

    s_portalTimer.restart();

    QString token = QUuid::createUuid().toString().remove('{').remove('}').remove('-');

    QVariantMap options;
    options["handle_token"] = token;
    options["interactive"] = false;

    // Build a non-empty parent_window handle. xdg-desktop-portal-gnome
    // (>= 46) rejects an empty string with "Failed to associate portal
    // window with parent window ''" when launched from custom global shortcuts.
    // On Wayland we pass "wayland:" prefix so it proceeds without requiring a focused window.
    QString parentWindow = "wayland:";

    // Use asynchronous DBus call so the main Qt GUI thread is not blocked
    m_portal->asyncCall("Screenshot", parentWindow, options);
}

void ScreenCapturer::onPortalResponse(uint response, const QVariantMap &results) {
    qint64 portalMs = s_portalTimer.elapsed();
    m_isCapturing = false;

    if (response != 0) {
        emit captureFailed("用户取消或系统拒绝截图请求 (Response Code: " + QString::number(response) + ")");
        return;
    }

    if (!results.contains("uri")) {
        emit captureFailed("未在 Portal 响应中找到图片路径");
        return;
    }

    QElapsedTimer loadTimer;
    loadTimer.start();

    QString uriStr = results.value("uri").toString();
    QUrl url(uriStr);
    QString localPath = url.toLocalFile();

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit captureFailed("无法读取截取的图像文件: " + localPath);
        return;
    }

    // Zero-overhead memory-mapped loading: bypass multiple kernel buffers
    uchar *mapPtr = file.map(0, file.size());
    QImage image;
    if (mapPtr) {
        image.loadFromData(mapPtr, file.size(), "PNG");
        file.unmap(mapPtr);
    } else {
        image.load(&file, "PNG");
    }
    file.close();

    // Clean up temporary image file generated by portal asynchronously
    QThreadPool::globalInstance()->start([localPath]() {
        QFile::remove(localPath);
    });

    qDebug() << "[PERF] Portal roundtrip:" << portalMs << "ms, Image decode:" << loadTimer.elapsed() << "ms";

    if (image.isNull()) {
        emit captureFailed("无法解析截取的图像文件: " + localPath);
    } else {
        emit screenCaptured(image);
    }
}
