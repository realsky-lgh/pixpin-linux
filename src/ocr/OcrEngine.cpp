#include "OcrEngine.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QDebug>
#include <QPainter>
#include <algorithm>

OcrEngine::OcrEngine(QObject *parent)
    : QObject(parent)
    , m_tessApi(std::make_unique<tesseract::TessBaseAPI>()) {
    initialize("chi_sim+eng");
    startRapidOcrWorker();
}

OcrEngine::~OcrEngine() {
    if (m_rapidOcrProc && m_rapidOcrProc->state() == QProcess::Running) {
        m_rapidOcrProc->write("QUIT\n");
        m_rapidOcrProc->waitForFinished(1000);
        m_rapidOcrProc->kill();
    }
    if (m_tessApi && m_tessInitialized) {
        m_tessApi->End();
    }
}

#include <QCoreApplication>
#include <QStandardPaths>

void OcrEngine::startRapidOcrWorker() {
    QStringList scriptCandidates = {
        QDir::homePath() + "/.local/share/pixpin/ocr_worker.py",
        QCoreApplication::applicationDirPath() + "/../share/pixpin/ocr_worker.py",
        "/usr/share/pixpin/ocr_worker.py",
        "/usr/local/share/pixpin/ocr_worker.py",
        QCoreApplication::applicationDirPath() + "/ocr_worker.py",
        QCoreApplication::applicationDirPath() + "/../src/ocr/ocr_worker.py"
    };

    QString scriptPath;
    for (const QString &path : scriptCandidates) {
        if (QFileInfo::exists(path)) {
            scriptPath = path;
            break;
        }
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "RapidOCR worker script not found in any standard paths.";
        m_rapidOcrAvailable = false;
        return;
    }

    QStringList pyCandidates = {
        QDir::homePath() + "/.local/share/pixpin/venv/bin/python",
        QDir::homePath() + "/.local/share/pixpin/venv/bin/python3",
        QStandardPaths::findExecutable("python3"),
        "/usr/bin/python3"
    };

    QString pyPath;
    for (const QString &path : pyCandidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            pyPath = path;
            break;
        }
    }

    if (pyPath.isEmpty()) {
        qWarning() << "Python executable not found for RapidOCR.";
        m_rapidOcrAvailable = false;
        return;
    }

    m_rapidOcrProc = std::make_unique<QProcess>();
    m_rapidOcrProc->setProgram(pyPath);
    m_rapidOcrProc->setArguments(QStringList() << scriptPath);
    m_rapidOcrProc->start();

    if (!m_rapidOcrProc->waitForStarted(2000)) {
        qWarning() << "Failed to start RapidOCR worker process";
        m_rapidOcrAvailable = false;
        return;
    }

    // Wait for READY signal
    if (m_rapidOcrProc->waitForReadyRead(5000)) {
        QByteArray line = m_rapidOcrProc->readLine().trimmed();
        if (line == "READY") {
            m_rapidOcrAvailable = true;
            qDebug() << "RapidOCR (PaddleOCR/ONNX) worker initialized successfully from" << scriptPath << "using" << pyPath;
        }
    }
}

bool OcrEngine::initialize(const QString &languages) {
    if (!m_tessApi) return false;

    int ret = m_tessApi->Init(nullptr, languages.toUtf8().constData(), tesseract::OEM_LSTM_ONLY);
    if (ret != 0) {
        ret = m_tessApi->Init(nullptr, "eng", tesseract::OEM_LSTM_ONLY);
    }

    if (ret == 0) {
        m_tessApi->SetVariable("user_defined_dpi", "300");
        m_tessApi->SetPageSegMode(tesseract::PSM_AUTO);
        m_tessInitialized = true;
    } else {
        m_tessInitialized = false;
    }

    return m_tessInitialized;
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
                continue;
            }
        }
        result.append(input[i]);
    }
    return result;
}
} // namespace

QString OcrEngine::recognizeWithRapidOcr(const QImage &image) {
    if (!m_rapidOcrProc || m_rapidOcrProc->state() != QProcess::Running) {
        startRapidOcrWorker();
        if (!m_rapidOcrAvailable) return QString();
    }

    // Encode to in-memory Base64 PNG for zero disk I/O
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    QByteArray b64 = bytes.toBase64();
    m_rapidOcrProc->write("B64:" + b64 + "\n");

    if (!m_rapidOcrProc->waitForReadyRead(8000)) {
        qWarning() << "RapidOCR worker response timed out";
        return QString();
    }

    QByteArray respData = m_rapidOcrProc->readLine();
    QJsonDocument doc = QJsonDocument::fromJson(respData);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.value("status").toString() == "ok") {
            return obj.value("text").toString().trimmed();
        }
    }

    return QString();
}

QString OcrEngine::recognizeWithTesseract(const QImage &image) {
    if (!m_tessInitialized || image.isNull()) {
        return QString();
    }

    QImage processed = image;
    if (processed.height() < 60 || processed.width() < 120) {
        processed = processed.scaled(processed.size() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QImage gray = processed.convertToFormat(QImage::Format_Grayscale8);

    // Contrast stretch and dark mode normalization
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
    if (meanLuma < 125.0) {
        gray.invertPixels();
        uchar oldMin = minVal;
        minVal = 255 - maxVal;
        maxVal = 255 - oldMin;
    }

    if (maxVal > minVal && (maxVal - minVal) < 200) {
        for (int y = 0; y < gray.height(); ++y) {
            uchar *line = gray.scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                int stretched = (line[x] - minVal) * 255 / (maxVal - minVal);
                line[x] = static_cast<uchar>(std::clamp(stretched, 0, 255));
            }
        }
    }

    m_tessApi->SetImage(gray.bits(), gray.width(), gray.height(), 1, gray.bytesPerLine());
    m_tessApi->SetSourceResolution(300);
    m_tessApi->Recognize(nullptr);

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

                    if (conf < 38.0f) continue;
                    if (w.length() <= 2 && (wordW > 45 && wordH > 45)) continue;

                    if (!lineText.isEmpty()) lineText += " ";
                    lineText += w;
                }

                if (ri->IsAtFinalElement(tesseract::RIL_TEXTLINE, tesseract::RIL_WORD)) break;
            } while (ri->Next(tesseract::RIL_WORD));

            lineText = formatCjkText(lineText.trimmed());
            if (lineText.length() == 1 && !lineText[0].isLetterOrNumber()) continue;
            if (!lineText.isEmpty()) {
                finalLines.append(lineText);
            }
        } while (ri->Next(tesseract::RIL_TEXTLINE));
    }

    m_tessApi->Clear();
    return finalLines.join("\n").trimmed();
}

QString OcrEngine::recognizeText(const QImage &image) {
    if (image.isNull()) return QString();

    QImage processed = image;
    if (processed.hasAlphaChannel()) {
        QImage whiteBg(processed.size(), QImage::Format_RGB888);
        whiteBg.fill(Qt::white);
        QPainter p(&whiteBg);
        p.drawImage(0, 0, processed);
        p.end();
        processed = whiteBg;
    }

    // Prefer RapidOCR (PaddleOCR/ONNX) for state-of-the-art scene/graphics text detection
    if (m_rapidOcrAvailable) {
        QString text = recognizeWithRapidOcr(processed);
        if (!text.isEmpty()) {
            return text;
        }
    }

    // Fallback to Tesseract
    return recognizeWithTesseract(processed);
}

QString OcrEngine::recognizeText(const QPixmap &pixmap) {
    return recognizeText(pixmap.toImage());
}
