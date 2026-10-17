#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>

class OcrResultDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcrResultDialog(const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);

private:
    void setupUI();
    void updateStats();

    QPlainTextEdit *m_textEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_btnCopyAll = nullptr;
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnClose = nullptr;
};
