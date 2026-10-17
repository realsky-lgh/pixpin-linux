#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <memory>

namespace tesseract {
    class TessBaseAPI;
}

class OcrEngine : public QObject {
    Q_OBJECT
public:
    explicit OcrEngine(QObject *parent = nullptr);
    ~OcrEngine() override;

    bool initialize(const QString &languages = "chi_sim+eng");
    QString recognizeText(const QImage &image);
    QString recognizeText(const QPixmap &pixmap);

private:
    std::unique_ptr<tesseract::TessBaseAPI> m_tessApi;
    bool m_initialized = false;
};
