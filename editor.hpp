#pragma once

#include "capture.hpp"

#include <QFutureWatcher>
#include <QLineEdit>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class QPainter;
class CaptureEditor final : public QWidget {
public:
  explicit CaptureEditor(CaptureData capture, QWidget *parent = nullptr);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

public:
  enum class Tool { Arrow, Marker, Rectangle, Text };

private:
  enum class Phase { Select, Edit };
  enum class OutputMode { Copy, Save, Both };

  struct ToolbarButton {
    QRectF rect;
    QString action;
    QString label;
    QColor color;
  };

  struct OcrResult {
    QString text;
    QString error;
  };

  [[nodiscard]] QRectF normalizedSelection(const QPointF &first, const QPointF &second) const;
  [[nodiscard]] QRectF editImageRect() const;
  [[nodiscard]] qreal editScale() const;
  [[nodiscard]] QPointF toAnnotationPoint(const QPointF &position) const;
  [[nodiscard]] int windowAt(const QPointF &position) const;
  [[nodiscard]] QVector<ToolbarButton> toolbarButtons() const;
  [[nodiscard]] QColor annotationColor() const;

  void acceptText();
  void beginText(const QPointF &point);
  void chooseWindow(int index);
  void finish(OutputMode mode);
  void handleToolbar(const QString &action);
  void paintEdit(QPainter &painter);
  void paintSelect(QPainter &painter);
  void runOcr();
  void setStatus(QString status);
  void updatePointerCursor();

  CaptureData capture_;
  Phase phase_ = Phase::Select;
  Tool tool_ = Tool::Arrow;
  QRectF selection_;
  QPointF dragStart_;
  QPointF cursor_;
  bool dragging_ = false;
  bool windowMode_ = false;
  bool backgroundEnabled_ = false;
  bool busy_ = false;
  int hoveredWindow_ = -1;
  int colorIndex_ = 0;
  int nextMarker_ = 1;
  qreal annotationSize_ = 4.0;
  QVector<Annotation> annotations_;
  QString status_ = QStringLiteral("Drag to select an area · Space selects a window");
  QLineEdit *textEditor_ = nullptr;
  QPointF textPoint_;
  QFutureWatcher<OcrResult> ocrWatcher_;
};
