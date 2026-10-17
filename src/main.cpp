#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTimer>
#include <QDebug>
#include "app/Application.h"

int main(int argc, char *argv[]) {
    // Enable High DPI scaling
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

    Application application;
    application.start();

    if (parser.isSet(captureOption)) {
        // Delay slightly to let event loop initialize
        QTimer::singleShot(100, &application, &Application::triggerCapture);
    } else if (parser.isSet(pinOption)) {
        QTimer::singleShot(100, &application, &Application::pinFromClipboard);
    }

    return app.exec();
}
