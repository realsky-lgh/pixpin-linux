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

    // Use LSTM OCR engine for high accuracy
    int ret = m_tessApi->Init(nullptr, languages.toUtf8().constData(), tesseract::OEM_LSTM_ONLY);
    if (ret != 0) {
        qWarning() << "Failed to init tesseract with" << languages << ", retrying with eng";
        ret = m_tessApi->Init(nullptr, "eng", tesseract::OEM_LSTM_ONLY);
    }

    if (ret == 0) {
        m_tessApi->SetVariable("user_defined_dpi", "300");
        m_tessApi->SetPageSegMode(tesseract::PSM_AUTO);
        m_initialized = true;
    } else {
        m_initialized = false;
    }

    return m_initialized;
}

QString OcrEngine::recognizeText(const QImage &image) {
    if (!m_initialized || image.isNull()) {
        return QString();
    }

    // Step 1: Normalize and upscale if image is small for better LSTM recognition
    QImage processed = image;
    if (processed.height() < 60 || processed.width() < 120) {
        processed = processed.scaled(processed.size() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Step 2: Convert to 8-bit Grayscale
    QImage gray = processed.convertToFormat(QImage::Format_Grayscale8);

    // Step 3: Dark Mode Detection
    // Tesseract LSTM model is trained on black text on white backgrounds.
    // Invert pixels if the image has a dark background.
    quint64 totalLuma = 0;
    int pixelCount = gray.width() * gray.height();
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *line = gray.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            totalLuma += line[x];
        }
    }

    double meanLuma = (double)totalLuma / pixelCount;
    if (meanLuma < 120.0) {
        gray.invertPixels();
    }

    // Step 4: Run Tesseract with 300 DPI hint
    m_tessApi->SetSourceResolution(300);
    m_tessApi->SetImage(gray.bits(), gray.width(), gray.height(), 1, gray.bytesPerLine());

    char *outText = m_tessApi->GetUTF8Text();
    QString result;
    if (outText) {
        result = QString::fromUtf8(outText).trimmed();
        delete[] outText;
    }

    m_tessApi->Clear();

    return result;
}

QString OcrEngine::recognizeText(const QPixmap &pixmap) {
    return recognizeText(pixmap.toImage());
}
