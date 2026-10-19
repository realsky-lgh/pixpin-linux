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
    initialize("chi_sim+chi_tra+eng");
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

    QStringList candidateDirs = {
        QDir::homePath() + "/.local/share/pixpin/tessdata",
        QCoreApplication::applicationDirPath() + "/../share/pixpin/tessdata",
        QCoreApplication::applicationDirPath() + "/../resources/tessdata",
        QCoreApplication::applicationDirPath() + "/resources/tessdata",
        "/usr/share/pixpin/tessdata",
        "/usr/local/share/pixpin/tessdata",
        "/usr/share/tesseract-ocr/5/tessdata",
        "/usr/share/tesseract-ocr/4.00/tessdata",
        "/usr/share/tessdata"
    };

    QString tessdataDir;
    for (const QString &dir : candidateDirs) {
        if (QFileInfo::exists(dir + "/chi_tra.traineddata")) {
            tessdataDir = dir;
            break;
        } else if (tessdataDir.isEmpty() && (QFileInfo::exists(dir + "/chi_sim.traineddata") || QFileInfo::exists(dir + "/eng.traineddata"))) {
            tessdataDir = dir;
        }
    }

    const char *datapath = tessdataDir.isEmpty() ? nullptr : tessdataDir.toUtf8().constData();

    bool hasTra = !tessdataDir.isEmpty() && QFileInfo::exists(tessdataDir + "/chi_tra.traineddata");
    bool hasSim = !tessdataDir.isEmpty() && QFileInfo::exists(tessdataDir + "/chi_sim.traineddata");
    bool hasEng = !tessdataDir.isEmpty() && QFileInfo::exists(tessdataDir + "/eng.traineddata");

    QStringList langOrder;
    if (hasSim && hasTra && hasEng) {
        langOrder << "chi_sim+chi_tra+eng";
    } else if (hasTra && hasEng) {
        langOrder << "chi_tra+eng";
    } else if (hasSim && hasEng) {
        langOrder << "chi_sim+eng";
    }
    langOrder << languages << "chi_sim+chi_tra+eng" << "chi_tra+eng" << "chi_sim+eng" << "eng";
    langOrder.removeDuplicates();

    int ret = -1;
    for (const QString &lang : langOrder) {
        ret = m_tessApi->Init(datapath, lang.toUtf8().constData(), tesseract::OEM_LSTM_ONLY);
        if (ret == 0) {
            qDebug() << "Tesseract initialized successfully with datapath:" << (datapath ? datapath : "(system default)") << "languages:" << lang;
            m_tessInitialized = true;
            m_tessApi->SetPageSegMode(tesseract::PSM_AUTO);
            return true;
        }
    }

    // Fallback: try default system path if custom datapath failed
    if (datapath != nullptr) {
        for (const QString &lang : QStringList{"chi_sim+chi_tra+eng", "chi_tra+eng", "chi_sim+eng", "eng"}) {
            ret = m_tessApi->Init(nullptr, lang.toUtf8().constData(), tesseract::OEM_LSTM_ONLY);
            if (ret == 0) {
                qDebug() << "Tesseract initialized with system path, lang:" << lang;
                m_tessInitialized = true;
                m_tessApi->SetPageSegMode(tesseract::PSM_AUTO);
                return true;
            }
        }
    }

    m_tessInitialized = false;
    return false;
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

    QImage rgb = processed.convertToFormat(QImage::Format_RGB888);

    m_tessApi->SetImage(rgb.bits(), rgb.width(), rgb.height(), 3, rgb.bytesPerLine());
    m_tessApi->Recognize(nullptr);

    char *utf8Text = m_tessApi->GetUTF8Text();
    QString result;
    if (utf8Text) {
        result = QString::fromUtf8(utf8Text).trimmed();
        delete[] utf8Text;
    }
    m_tessApi->Clear();

    if (result.isEmpty()) {
        return QString();
    }

    QStringList lines = result.split('\n');
    QStringList cleanLines;
    for (const QString &line : lines) {
        QString trimmed = formatCjkText(line.trimmed());
        if (!trimmed.isEmpty()) {
            cleanLines.append(trimmed);
        }
    }
    return cleanLines.join("\n").trimmed();
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

    // Try Tesseract first (supports Simplified + Traditional Chinese + English at native speed)
    if (m_tessInitialized) {
        QString text = recognizeWithTesseract(processed);
        if (!text.isEmpty()) {
            return text;
        }
    }

    // Fallback: RapidOCR (PaddleOCR/ONNX) for complex natural scenes / stylized text
    if (m_rapidOcrAvailable) {
        QString text = recognizeWithRapidOcr(processed);
        if (!text.isEmpty()) {
            return text;
        }
    }

    return QString();
}

QString OcrEngine::recognizeText(const QPixmap &pixmap) {
    return recognizeText(pixmap.toImage());
}
