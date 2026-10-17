#include "OcrEngine.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QDebug>

OcrEngine::OcrEngine(QObject *parent)
    : QObject(parent)
    , m_tessApi(std::make_unique<tesseract::TessBaseAPI>()) {
    initialize("chi_sim+eng");
}

OcrEngine::~OcrEngine() {
    if (m_tessApi && m_initialized) {
        m_tessApi->End();
    }
}

bool OcrEngine::initialize(const QString &languages) {
    if (!m_tessApi) return false;

    int ret = m_tessApi->Init(nullptr, languages.toUtf8().constData(), tesseract::OEM_LSTM_ONLY);
    if (ret != 0) {
        qWarning() << "Failed to init tesseract with" << languages << ", retrying with eng";
        ret = m_tessApi->Init(nullptr, "eng", tesseract::OEM_LSTM_ONLY);
    }

    m_initialized = (ret == 0);
    return m_initialized;
}

QString OcrEngine::recognizeText(const QImage &image) {
    if (!m_initialized || image.isNull()) {
        return QString();
    }

    QImage rgbImg = image.convertToFormat(QImage::Format_RGB888);
    m_tessApi->SetImage(rgbImg.bits(), rgbImg.width(), rgbImg.height(), 3, rgbImg.bytesPerLine());

    char *outText = m_tessApi->GetUTF8Text();
    QString result;
    if (outText) {
        result = QString::fromUtf8(outText).trimmed();
        delete[] outText;
    }

    return result;
}

QString OcrEngine::recognizeText(const QPixmap &pixmap) {
    return recognizeText(pixmap.toImage());
}
