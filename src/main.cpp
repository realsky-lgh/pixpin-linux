#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QDebug>
#include <unistd.h>
#include "app/Application.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("PixPin Linux");
    app.setApplicationVersion(PIXPIN_VERSION);
    app.setOrganizationName("PixPin");
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription("PixPin Linux - 智能截图、贴图与离线 OCR 文本识别工具");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption captureOption(QStringList() << "c" << "capture", "立即启动截屏 (可绑定全局快捷键)");
    QCommandLineOption pinOption(QStringList() << "p" << "pin", "直接贴出剪贴板中的图片");
    parser.addOption(captureOption);
    parser.addOption(pinOption);

    parser.process(app);

    QString serverName = QString("pixpin_linux_ipc_%1").arg(getuid());

    // Check if an instance is already running
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(300)) {
        // An instance is already running in background! Send command and exit immediately.
        QString command = "capture";
        if (parser.isSet(pinOption)) {
            command = "pin";
        } else if (parser.isSet(captureOption)) {
            command = "capture";
        }

        socket.write(command.toUtf8());
        socket.flush();
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        // Exit this transient process so NO new tray icon is created!
        return 0;
    }

    // No existing instance running. This process becomes the primary daemon.
    QLocalServer::removeServer(serverName);
    auto *server = new QLocalServer(&app);
    if (!server->listen(serverName)) {
        qWarning() << "Failed to start local IPC server:" << server->errorString();
    }

    Application application;
    application.start();

    // Listen for commands from future shortcut/cli invocations
    QObject::connect(server, &QLocalServer::newConnection, [server, &application]() {
        QLocalSocket *client = server->nextPendingConnection();
        if (client) {
            QObject::connect(client, &QLocalSocket::readyRead, [client, &application]() {
                QString cmd = QString::fromUtf8(client->readAll()).trimmed();
                if (cmd == "capture") {
                    application.triggerCapture();
                } else if (cmd == "pin") {
                    application.pinFromClipboard();
                }
            });
        }
    });

    if (parser.isSet(captureOption)) {
        QTimer::singleShot(100, &application, &Application::triggerCapture);
    } else if (parser.isSet(pinOption)) {
        QTimer::singleShot(100, &application, &Application::pinFromClipboard);
    }

    return app.exec();
}
