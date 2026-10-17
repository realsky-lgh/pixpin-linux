#include "OcrEngine.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QStringList>
#include <QDebug>
#include <algorithm>

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

    // Use LSTM OCR engine for state-of-the-art accuracy
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

namespace {
bool isCjkChar(const QChar &ch) {
    ushort u = ch.unicode();
    return (u >= 0x4E00 && u <= 0x9FFF) || 
           (u >= 0x3400 && u <= 0x4DBF) || 
           (u >= 0x3000 && u <= 0x303F) || 
           (u >= 0xFF00 && u <= 0xFFEF);
}

QString formatCjkText(const QString &input) {
    QString result;
    result.reserve(input.length());
    for (int i = 0; i < input.length(); ++i) {
        if (input[i] == ' ' && i > 0 && i < input.length() - 1) {
            if (isCjkChar(input[i - 1]) && isCjkChar(input[i + 1])) {
                // Drop space between two CJK characters
                continue;
            }
        }
        result.append(input[i]);
    }
    return result;
}
} // namespace

QString OcrEngine::recognizeText(const QImage &image) {
    if (!m_initialized || image.isNull()) {
        return QString();
    }

    // Step 1: Preprocessing & upscale small text snippets
    QImage processed = image;
    if (processed.height() < 60 || processed.width() < 120) {
        processed = processed.scaled(processed.size() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Step 2: Convert to 8-bit Grayscale
    QImage gray = processed.convertToFormat(QImage::Format_Grayscale8);

    // Step 3: Contrast Stretch & Dark Mode Normalization
    uchar minVal = 255;
    uchar maxVal = 0;
    quint64 totalLuma = 0;
    int pixelCount = gray.width() * gray.height();

    for (int y = 0; y < gray.height(); ++y) {
        const uchar *line = gray.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            uchar v = line[x];
            totalLuma += v;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }
    }

    double meanLuma = (double)totalLuma / pixelCount;
    // If dark background (mean brightness < 125), invert so text is dark on white
    if (meanLuma < 125.0) {
        gray.invertPixels();
        uchar oldMin = minVal;
        minVal = 255 - maxVal;
        maxVal = 255 - oldMin;
    }

    // High dynamic range contrast stretch
    if (maxVal > minVal && (maxVal - minVal) < 200) {
        for (int y = 0; y < gray.height(); ++y) {
            uchar *line = gray.scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                int stretched = (line[x] - minVal) * 255 / (maxVal - minVal);
                line[x] = static_cast<uchar>(std::clamp(stretched, 0, 255));
            }
        }
    }

    // Step 4: Run Tesseract (SetImage BEFORE SetSourceResolution)
    m_tessApi->SetImage(gray.bits(), gray.width(), gray.height(), 1, gray.bytesPerLine());
    m_tessApi->SetSourceResolution(300);
    m_tessApi->Recognize(nullptr);

    // Step 5: Advanced Result Iterator - Filter non-text images, icons, and noise
    tesseract::ResultIterator *ri = m_tessApi->GetIterator();
    QStringList finalLines;

    if (ri) {
        do {
            QString lineText;
            do {
                const char *word = ri->GetUTF8Text(tesseract::RIL_WORD);
                float conf = ri->Confidence(tesseract::RIL_WORD);

                if (word) {
                    QString w = QString::fromUtf8(word).trimmed();
                    delete[] word;

                    int wx1, wy1, wx2, wy2;
                    ri->BoundingBox(tesseract::RIL_WORD, &wx1, &wy1, &wx2, &wy2);
                    int wordW = wx2 - wx1;
                    int wordH = wy2 - wy1;

                    // Filter 1: Low-confidence artifacts (< 38%)
                    if (conf < 38.0f) {
                        continue;
                    }

                    // Filter 2: Huge single/dual-glyph icons (e.g. decorative circles, avatars misidentified as 'O','Q','C')
                    if (w.length() <= 2 && (wordW > 45 && wordH > 45)) {
                        continue;
                    }

                    // Append word
                    if (!lineText.isEmpty()) {
                        lineText += " ";
                    }
                    lineText += w;
                }

                if (ri->IsAtFinalElement(tesseract::RIL_TEXTLINE, tesseract::RIL_WORD)) {
                    break;
                }
            } while (ri->Next(tesseract::RIL_WORD));

            // Clean CJK spacing and trim
            lineText = formatCjkText(lineText.trimmed());

            // Filter 3: Drop noise lines with only 1 isolated punctuation symbol
            if (lineText.length() == 1 && !lineText[0].isLetterOrNumber()) {
                continue;
            }

            if (!lineText.isEmpty()) {
                finalLines.append(lineText);
            }
        } while (ri->Next(tesseract::RIL_TEXTLINE));
    }

    m_tessApi->Clear();

    return finalLines.join("\n").trimmed();
}

QString OcrEngine::recognizeText(const QPixmap &pixmap) {
    return recognizeText(pixmap.toImage());
}
