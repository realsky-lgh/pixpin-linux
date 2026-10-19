#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QProcess>
#include <memory>

namespace tesseract {
    class TessBaseAPI;
}

class OcrEngine : public QObject {
    Q_OBJECT
public:
    explicit OcrEngine(QObject *parent = nullptr);
    ~OcrEngine() override;

    bool initialize(const QString &languages = "chi_sim+chi_tra+eng");
    QString recognizeText(const QImage &image);
    QString recognizeText(const QPixmap &pixmap);

private:
    void startRapidOcrWorker();
    QString recognizeWithRapidOcr(const QImage &image);
    QString recognizeWithTesseract(const QImage &image);

    std::unique_ptr<tesseract::TessBaseAPI> m_tessApi;
    std::unique_ptr<QProcess> m_rapidOcrProc;
    bool m_tessInitialized = false;
    bool m_rapidOcrAvailable = false;
};
