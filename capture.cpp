#include "capture.hpp"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>

#include <algorithm>
#include <cmath>

QFont annotationTextFont(qreal size) {
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setWeight(QFont::DemiBold);
  font.setItalic(false);
  font.setPixelSize(qRound(std::max<qreal>(18.0, size * 5.0)));
  return font;
}

namespace {
struct ProcessResult {
  QByteArray output;
  QByteArray error;
  int exitCode = -1;
  bool finished = false;
};

ProcessResult runProcess(const QString &program, const QStringList &arguments,
                         const QByteArray &input = {}, int timeoutMs = 10000) {
  QProcess process;
  process.setProcessChannelMode(QProcess::SeparateChannels);
  process.start(program, arguments);
  if (!process.waitForStarted(2000))
    return {{}, process.errorString().toUtf8(), -1, false};

  if (!input.isEmpty())
    process.write(input);
  process.closeWriteChannel();
  const bool finished = process.waitForFinished(timeoutMs);
  if (!finished)
    process.kill();
  return {process.readAllStandardOutput(), process.readAllStandardError(),
          finished ? process.exitCode() : -1, finished};
}

QString runtimePath(const QString &name) {
  QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty())
    runtime = QDir::tempPath();
  return QDir(runtime).filePath(name);
}

bool parseMonitor(const QByteArray &json, MonitorInfo &monitor, QString &error) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    error = QStringLiteral("Could not parse Hyprland monitors: %1").arg(parseError.errorString());
    return false;
  }

  for (const QJsonValue value : document.array()) {
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("focused")).toBool())
      continue;

    const qreal scale = object.value(QStringLiteral("scale")).toDouble(1.0);
    const int rawWidth = object.value(QStringLiteral("width")).toInt();
    const int rawHeight = object.value(QStringLiteral("height")).toInt();
    const int transform = object.value(QStringLiteral("transform")).toInt();
    int logicalWidth = static_cast<int>(std::floor(rawWidth / std::max<qreal>(scale, 0.01)));
    int logicalHeight = static_cast<int>(std::floor(rawHeight / std::max<qreal>(scale, 0.01)));
    if (transform == 1 || transform == 3)
      std::swap(logicalWidth, logicalHeight);

    monitor.name = object.value(QStringLiteral("name")).toString();
    monitor.geometry = {object.value(QStringLiteral("x")).toInt(),
                        object.value(QStringLiteral("y")).toInt(),
                        logicalWidth, logicalHeight};
    monitor.pixelSize = {rawWidth, rawHeight};
    monitor.scale = scale;
    monitor.workspaceId = object.value(QStringLiteral("activeWorkspace"))
                              .toObject()
                              .value(QStringLiteral("id"))
                              .toInt();
    return !monitor.name.isEmpty() && logicalWidth > 0 && logicalHeight > 0;
  }

  error = QStringLiteral("Hyprland did not report a focused monitor");
  return false;
}

QVector<WindowTarget> parseWindows(const QByteArray &json, const MonitorInfo &monitor) {
  QVector<WindowTarget> result;
  const QJsonDocument document = QJsonDocument::fromJson(json);
  if (!document.isArray())
    return result;

  for (const QJsonValue value : document.array()) {
    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("workspace")).toObject().value(QStringLiteral("id")).toInt() !=
        monitor.workspaceId)
      continue;

    const QJsonArray at = object.value(QStringLiteral("at")).toArray();
    const QJsonArray size = object.value(QStringLiteral("size")).toArray();
    if (at.size() < 2 || size.size() < 2)
      continue;

    QRect rect(at.at(0).toInt() - monitor.geometry.x(),
               at.at(1).toInt() - monitor.geometry.y(), size.at(0).toInt(), size.at(1).toInt());
    rect = rect.intersected(QRect(QPoint(), monitor.geometry.size()));
    if (rect.isEmpty())
      continue;

    QString title = object.value(QStringLiteral("title")).toString();
    if (title.isEmpty())
      title = object.value(QStringLiteral("class")).toString(QStringLiteral("window"));
    result.push_back(
        {rect, object.value(QStringLiteral("stableId")).toString(), std::move(title)});
  }
  return result;
}

void drawAnnotation(QPainter &painter, const Annotation &annotation) {
  const qreal width = std::max<qreal>(2.0, annotation.size);
  QPen pen(annotation.color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(annotation.color);

  if (annotation.kind == Annotation::Kind::Rectangle) {
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(annotation.start, annotation.end).normalized());
    return;
  }

  if (annotation.kind == Annotation::Kind::Arrow) {
    const QLineF line(annotation.start, annotation.end);
    if (line.length() < 1.0)
      return;
    const qreal angle = std::atan2(line.dy(), line.dx());
    const qreal headLength = std::max<qreal>(14.0, annotation.size * 4.2);
    const qreal halfWidth = headLength * 0.46;
    const QPointF direction(std::cos(angle), std::sin(angle));
    const QPointF perpendicular(-direction.y(), direction.x());
    const QPointF base = annotation.end - direction * headLength;
    const QPointF stemEnd = annotation.end - direction * (headLength * 0.5);
    painter.drawLine(annotation.start, stemEnd);
    QPolygonF head;
    head << annotation.end << base + perpendicular * halfWidth << base - perpendicular * halfWidth;
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(head);
    return;
  }

  if (annotation.kind == Annotation::Kind::Marker) {
    const qreal diameter = std::max<qreal>(24.0, annotation.size * 6.0);
    const QRectF marker(annotation.start.x() - diameter / 2.0,
                        annotation.start.y() - diameter / 2.0, diameter, diameter);
    painter.setPen(QPen(Qt::white, std::max<qreal>(1.0, annotation.size * 0.35)));
    painter.setBrush(annotation.color);
    painter.drawEllipse(marker);
    QFont font(QStringLiteral("Noto Sans"));
    font.setBold(true);
    font.setPixelSize(static_cast<int>(std::max<qreal>(11.0, annotation.size * 3.2)));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(marker, Qt::AlignCenter, QString::number(annotation.number));
    return;
  }

  const QFont font = annotationTextFont(annotation.size);
  QPainterPath textPath;
  textPath.addText(annotation.start, font, annotation.text);
  painter.setBrush(annotation.color);
  painter.setPen(QPen(QColor(0, 0, 0, 145), std::max<qreal>(1.0, annotation.size * 0.3),
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPath(textPath);
}

QRect pixelSelection(const CaptureData &capture, const QRectF &selection) {
  const QRectF bounded = selection.normalized().intersected(QRectF(QPointF(), capture.preview.size()));
  const qreal scaleX = capture.source.width() / static_cast<qreal>(capture.preview.width());
  const qreal scaleY = capture.source.height() / static_cast<qreal>(capture.preview.height());
  const int left = std::clamp(static_cast<int>(std::floor(bounded.left() * scaleX)), 0,
                              capture.source.width());
  const int top = std::clamp(static_cast<int>(std::floor(bounded.top() * scaleY)), 0,
                             capture.source.height());
  const int right = std::clamp(static_cast<int>(std::ceil(bounded.right() * scaleX)), left,
                               capture.source.width());
  const int bottom = std::clamp(static_cast<int>(std::ceil(bounded.bottom() * scaleY)), top,
                                capture.source.height());
  return QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
}

bool encodePng(const QImage &image, QByteArray &png, QString &error) {
  QBuffer buffer(&png);
  if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
    error = QStringLiteral("Could not encode screenshot as PNG");
    return false;
  }
  return true;
}
} // namespace

void paintAnnotation(QPainter &painter, const Annotation &annotation) {
  drawAnnotation(painter, annotation);
}

bool captureFocusedMonitor(CaptureData &capture, QString &error) {
  const ProcessResult monitors = runProcess(QStringLiteral("hyprctl"),
                                            {QStringLiteral("monitors"), QStringLiteral("-j")});
  if (!monitors.finished || monitors.exitCode != 0 ||
      !parseMonitor(monitors.output, capture.monitor, error)) {
    if (error.isEmpty())
      error = QString::fromUtf8(monitors.error).trimmed();
    return false;
  }

  QTemporaryFile sourceFile(runtimePath(QStringLiteral("omarchy-capture-XXXXXX.ppm")));
  sourceFile.setAutoRemove(true);
  if (!sourceFile.open()) {
    error = sourceFile.errorString();
    return false;
  }
  const QString sourcePath = sourceFile.fileName();
  sourceFile.close();

  QProcess freeze;
  freeze.setProcessChannelMode(QProcess::ForwardedErrorChannel);
  freeze.start(QStringLiteral("hyprpicker"), {QStringLiteral("-r"), QStringLiteral("-z")});
  freeze.waitForStarted(500);
  QThread::msleep(100);

  const QRect geometry = capture.monitor.geometry;
  const QString grimGeometry = QStringLiteral("%1,%2 %3x%4")
                                   .arg(geometry.x())
                                   .arg(geometry.y())
                                   .arg(geometry.width())
                                   .arg(geometry.height());
  const ProcessResult grim = runProcess(QStringLiteral("grim"),
                                        {QStringLiteral("-t"), QStringLiteral("ppm"),
                                         QStringLiteral("-g"), grimGeometry, sourcePath}, {}, 10000);
  freeze.terminate();
  if (!freeze.waitForFinished(300)) {
    freeze.kill();
    freeze.waitForFinished(300);
  }

  if (!grim.finished || grim.exitCode != 0 || !capture.source.load(sourcePath)) {
    error = QStringLiteral("Screen capture failed: %1").arg(QString::fromUtf8(grim.error).trimmed());
    return false;
  }

  capture.preview = capture.source.scaled(geometry.size(), Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation);
  if (capture.preview.isNull()) {
    error = QStringLiteral("Could not prepare screenshot preview");
    return false;
  }

  const ProcessResult clients = runProcess(QStringLiteral("hyprctl"),
                                           {QStringLiteral("clients"), QStringLiteral("-j")});
  if (clients.finished && clients.exitCode == 0)
    capture.windows = parseWindows(clients.output, capture.monitor);
  return true;
}

QImage renderCapture(const CaptureData &capture, const QRectF &selection,
                     const QVector<Annotation> &annotations, bool backgroundEnabled) {
  const QRect pixels = pixelSelection(capture, selection);
  if (pixels.isEmpty())
    return {};

  const QImage cropped = capture.source.copy(pixels).convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const qreal scaleX = capture.source.width() / static_cast<qreal>(capture.preview.width());
  const qreal scaleY = capture.source.height() / static_cast<qreal>(capture.preview.height());
  const int marginX = backgroundEnabled ? static_cast<int>(std::round(48.0 * scaleX)) : 0;
  const int marginY = backgroundEnabled ? static_cast<int>(std::round(48.0 * scaleY)) : 0;
  QImage output(cropped.width() + marginX * 2, cropped.height() + marginY * 2,
                QImage::Format_ARGB32_Premultiplied);
  output.fill(Qt::transparent);

  QPainter painter(&output);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                         QPainter::TextAntialiasing);
  if (backgroundEnabled) {
    QLinearGradient gradient(0, 0, output.width(), output.height());
    gradient.setColorAt(0.0, QColor(QStringLiteral("#1d2030")));
    gradient.setColorAt(1.0, QColor(QStringLiteral("#364f78")));
    painter.fillRect(output.rect(), gradient);
    const QRectF imageRect(marginX, marginY, cropped.width(), cropped.height());
    for (int layer = 18; layer > 0; --layer) {
      const qreal spread = layer * std::max(scaleX, scaleY) * 0.9;
      QColor shadow(0, 0, 0, std::max(2, 26 - layer));
      painter.setPen(Qt::NoPen);
      painter.setBrush(shadow);
      painter.drawRoundedRect(imageRect.adjusted(-spread, -spread + 10 * scaleY,
                                                  spread, spread + 10 * scaleY),
                              14 * scaleX + spread, 14 * scaleY + spread);
    }
    QPainterPath clip;
    clip.addRoundedRect(imageRect, 14 * scaleX, 14 * scaleY);
    painter.save();
    painter.setClipPath(clip);
    painter.drawImage(imageRect.topLeft(), cropped);
    painter.restore();
  } else {
    painter.drawImage(QPoint(0, 0), cropped);
  }

  painter.save();
  painter.translate(marginX, marginY);
  painter.scale(scaleX, scaleY);
  for (const Annotation &annotation : annotations)
    drawAnnotation(painter, annotation);
  painter.restore();
  painter.end();
  return output;
}

bool copyPngToClipboard(const QImage &image, QString &error) {
  if (image.isNull()) {
    error = QStringLiteral("Screenshot is empty");
    return false;
  }
  QApplication::clipboard()->setImage(image);

  QByteArray png;
  if (!encodePng(image, png, error))
    return false;
  const ProcessResult copied = runProcess(QStringLiteral("wl-copy"),
                                          {QStringLiteral("--type"), QStringLiteral("image/png")},
                                          png, 5000);
  if (!copied.finished || copied.exitCode != 0) {
    error = QStringLiteral("Could not persist image clipboard: %1")
                .arg(QString::fromUtf8(copied.error).trimmed());
    return false;
  }
  return true;
}

QString saveScreenshot(const QImage &image, QString &error) {
  QString root = qEnvironmentVariable("OMARCHY_SCREENSHOT_DIR");
  if (root.isEmpty())
    root = QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
               .filePath(QStringLiteral("Screenshots"));
  if (!QDir().mkpath(root)) {
    error = QStringLiteral("Could not create screenshot directory: %1").arg(root);
    return {};
  }

  const QString stem = QStringLiteral("screenshot-%1")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
  QString path = QDir(root).filePath(stem + QStringLiteral(".png"));
  for (int suffix = 2; QFile::exists(path); ++suffix)
    path = QDir(root).filePath(QStringLiteral("%1-%2.png").arg(stem).arg(suffix));
  if (!image.save(path, "PNG")) {
    error = QStringLiteral("Could not save screenshot: %1").arg(path);
    return {};
  }
  return path;
}

bool copyTextToClipboard(const QString &text, QString &error) {
  if (text.isEmpty()) {
    error = QStringLiteral("No text found in selection");
    return false;
  }
  QApplication::clipboard()->setText(text);
  const ProcessResult copied = runProcess(
      QStringLiteral("wl-copy"),
      {QStringLiteral("--type"), QStringLiteral("text/plain;charset=utf-8")},
      text.toUtf8(), 5000);
  if (!copied.finished || copied.exitCode != 0) {
    error = QStringLiteral("Could not persist text clipboard: %1")
                .arg(QString::fromUtf8(copied.error).trimmed());
    return false;
  }
  return true;
}

QString recognizeText(const QImage &image, QString &error) {
  QTemporaryFile input(runtimePath(QStringLiteral("omarchy-ocr-XXXXXX.png")));
  input.setAutoRemove(true);
  if (!input.open()) {
    error = input.errorString();
    return {};
  }
  const QString path = input.fileName();
  input.close();
  if (!image.save(path, "PNG")) {
    error = QStringLiteral("Could not prepare image for OCR");
    return {};
  }

  const QString languages = qEnvironmentVariable("OMARCHY_OCR_LANGS", QStringLiteral("eng"));
  const ProcessResult result = runProcess(
      QStringLiteral("tesseract"),
      {path, QStringLiteral("stdout"), QStringLiteral("--oem"), QStringLiteral("1"),
       QStringLiteral("--psm"), QStringLiteral("6"), QStringLiteral("-l"), languages,
       QStringLiteral("--dpi"), QStringLiteral("300"), QStringLiteral("-c"),
       QStringLiteral("preserve_interword_spaces=1")},
      {}, 30000);
  if (!result.finished || result.exitCode != 0) {
    error = QStringLiteral("OCR failed: %1").arg(QString::fromUtf8(result.error).trimmed());
    return {};
  }
  const QString text = QString::fromUtf8(result.output).trimmed();
  if (text.isEmpty())
    error = QStringLiteral("No text found in selection");
  return text;
}

void sendCaptureNotification(const QString &message, const QString &imagePath) {
  QStringList arguments{QStringLiteral("-g"), QStringLiteral(""), message,
                        QStringLiteral("-t"), QStringLiteral("2200")};
  if (!imagePath.isEmpty())
    arguments << QStringLiteral("--image") << imagePath;
  QProcess::startDetached(QStringLiteral("omarchy-notification-send"), arguments);
}
