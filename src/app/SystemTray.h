#pragma once

#include <QSystemTrayIcon>
#include <QMenu>

class SystemTray : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit SystemTray(QObject *parent = nullptr);

signals:
    void captureRequested();
    void pinClipboardRequested();
    void quitRequested();

private:
    void setupUI();
    QMenu *m_menu = nullptr;
};
