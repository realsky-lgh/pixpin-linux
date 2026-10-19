#include "OcrResultDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>

#include <QShortcut>
#include <QKeyEvent>

OcrResultDialog::OcrResultDialog(const QString &text, bool autoCopied, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("PixPin - 离线文字识别 (OCR)");
    resize(560, 400);
    setupUI();
    setText(text, autoCopied);
}

void OcrResultDialog::setupUI() {
    setStyleSheet(
        "QDialog {"
        "   background-color: #1E1E22;"
        "   color: #ECECEC;"
        "}"
        "QPlainTextEdit {"
        "   background-color: #28282C;"
        "   color: #F5F5F7;"
        "   border: 1px solid rgba(255, 255, 255, 0.15);"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "   font-family: 'Monospace', 'Sans';"
        "   font-size: 13px;"
        "   line-height: 1.5;"
        "}"
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 0.1);"
        "   color: white;"
        "   border: 1px solid rgba(255, 255, 255, 0.2);"
        "   border-radius: 6px;"
        "   padding: 6px 16px;"
        "   font-size: 13px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 0.2);"
        "}"
        "QPushButton#btnCopyAll {"
        "   background-color: #007AFF;"
        "   border: none;"
        "}"
        "QPushButton#btnCopyAll:hover {"
        "   background-color: #0A84FF;"
        "}"
        "QLabel {"
        "   color: #A0A0A5;"
        "   font-size: 12px;"
        "}"
    );

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *titleLabel = new QLabel("🔤 识别结果 (支持简繁中文及英文，可直接编辑修改)：", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FFFFFF;");
    rootLayout->addWidget(titleLabel);

    m_textEdit = new QPlainTextEdit(this);
    connect(m_textEdit, &QPlainTextEdit::textChanged, [this]() {
        m_btnCopyAll->setText("📋 复制全部 (Shift+C)");
        updateStats();
    });
    rootLayout->addWidget(m_textEdit);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_statusLabel = new QLabel(this);
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

    m_btnCopyAll = new QPushButton("📋 复制全部 (Shift+C)", this);
    m_btnCopyAll->setObjectName("btnCopyAll");
    m_btnSave = new QPushButton("💾 保存为 .txt", this);
    m_btnClose = new QPushButton("关闭 (Esc)", this);

    bottomLayout->addWidget(m_btnCopyAll);
    bottomLayout->addWidget(m_btnSave);
    bottomLayout->addWidget(m_btnClose);

    rootLayout->addLayout(bottomLayout);

    connect(m_btnCopyAll, &QPushButton::clicked, this, &OcrResultDialog::copyAll);

    // Global dialog shortcuts for copying text
    auto *copyShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_C), this);
    connect(copyShortcut, &QShortcut::activated, [this]() {
        if (!m_textEdit->hasFocus()) {
            copyAll();
        }
    });

    auto *ctrlShiftC = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    connect(ctrlShiftC, &QShortcut::activated, this, &OcrResultDialog::copyAll);

    connect(m_btnSave, &QPushButton::clicked, [this]() {
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QString fileName = QString("PixPin_OCR_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        QString path = QFileDialog::getSaveFileName(this, "保存文本", defaultPath + "/" + fileName, "文本文件 (*.txt)");
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << m_textEdit->toPlainText();
            }
        }
    });

    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void OcrResultDialog::setText(const QString &text, bool autoCopied) {
    m_textEdit->setPlainText(text);
    updateStats();
    if (autoCopied) {
        m_btnCopyAll->setText("✅ 已自动复制！(Shift+C)");
    } else {
        m_btnCopyAll->setText("📋 复制全部 (Shift+C)");
    }
}

void OcrResultDialog::copyAll() {
    QApplication::clipboard()->setText(m_textEdit->toPlainText());
    m_btnCopyAll->setText("✅ 已复制到剪贴板！(Shift+C)");
}

void OcrResultDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ShiftModifier) && !m_textEdit->hasFocus()) {
        copyAll();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void OcrResultDialog::updateStats() {
    QString content = m_textEdit->toPlainText();
    int chars = content.length();
    int lines = content.isEmpty() ? 0 : content.count('\n') + 1;
    m_statusLabel->setText(QString("统计: %1 字符 / %2 行").arg(chars).arg(lines));
}
