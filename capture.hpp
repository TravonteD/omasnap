#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

class QFont;
class QPainter;

struct MonitorInfo {
  QString name;
  QRect geometry;
  QSize pixelSize;
  qreal scale = 1.0;
  int workspaceId = 0;
};

struct WindowTarget {
  QRect rect;
  QString stableId;
  QString title;
};

struct CaptureData {
  MonitorInfo monitor;
  QImage source;
  QImage preview;
  QVector<WindowTarget> windows;
};

struct Annotation {
  enum class Kind { Arrow, Marker, Rectangle, Text };

  Kind kind = Kind::Arrow;
  QPointF start;
  QPointF end;
  QString text;
  QColor color;
  qreal size = 4.0;
  int number = 0;
};

[[nodiscard]] QFont annotationTextFont(qreal size);
[[nodiscard]] bool captureFocusedMonitor(CaptureData &capture, QString &error);
[[nodiscard]] bool captureWindowSurface(const WindowTarget &window, QImage &image,
                                        QString &error);
[[nodiscard]] QImage renderCapture(const CaptureData &capture, const QRectF &selection,
                                   const QVector<Annotation> &annotations,
                                   bool backgroundEnabled);
[[nodiscard]] bool copyPngToClipboard(const QImage &image, QString &error);
[[nodiscard]] bool copyTextToClipboard(const QString &text, QString &error);
void paintAnnotation(QPainter &painter, const Annotation &annotation);
[[nodiscard]] QString saveScreenshot(const QImage &image, QString &error);
[[nodiscard]] QString recognizeText(const QImage &image, QString &error);
void sendCaptureNotification(const QString &message, const QString &imagePath = {});
