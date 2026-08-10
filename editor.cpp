#include "editor.hpp"

#include <QtConcurrent/QtConcurrentRun>

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QCursor>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <limits>
#include <cmath>
#include <utility>

namespace {
constexpr std::array<const char *, 6> kColorNames{
    "#ff375f", "#ff9f0a", "#ffd60a", "#30d158", "#0a84ff", "#bf5af2"};

QString toolAction(CaptureEditor::Tool tool) {
  switch (tool) {
  case CaptureEditor::Tool::Arrow:
    return QStringLiteral("tool-arrow");
  case CaptureEditor::Tool::Marker:
    return QStringLiteral("tool-marker");
  case CaptureEditor::Tool::Rectangle:
    return QStringLiteral("tool-rectangle");
  case CaptureEditor::Tool::Text:
    return QStringLiteral("tool-text");
  }
  return {};
}

void drawStatusPill(QPainter &painter, const QRect &bounds, const QString &text) {
  QFont font(QStringLiteral("Noto Sans"));
  font.setPixelSize(13);
  painter.setFont(font);
  const int width = painter.fontMetrics().horizontalAdvance(text) + 28;
  const QRectF pill((bounds.width() - width) / 2.0, bounds.height() - 42.0, width, 30);
  painter.setPen(QPen(QColor(255, 255, 255, 32), 1));
  painter.setBrush(QColor(18, 18, 22, 232));
  painter.drawRoundedRect(pill, 10, 10);
  painter.setPen(Qt::white);
  painter.drawText(pill, Qt::AlignCenter, text);
}

void drawToolbarIcon(QPainter &painter, const QRectF &bounds, const QString &action,
                     const QString &label, const QColor &color) {
  if (action == QStringLiteral("size")) {
    painter.setPen(color);
    painter.drawText(bounds, Qt::AlignCenter, label);
    return;
  }

  painter.save();
  constexpr qreal iconSize = 19.0;
  painter.translate(bounds.center().x() - iconSize / 2.0,
                    bounds.center().y() - iconSize / 2.0);
  painter.scale(iconSize / 24.0, iconSize / 24.0);
  painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  if (action == QStringLiteral("tool-arrow")) {
    painter.drawLine(QPointF(7, 17), QPointF(17, 7));
    painter.drawLine(QPointF(7, 7), QPointF(17, 7));
    painter.drawLine(QPointF(17, 7), QPointF(17, 17));
  } else if (action == QStringLiteral("tool-marker")) {
    painter.drawEllipse(QPointF(12, 12), 8, 8);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(11);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(4, 4, 16, 16), Qt::AlignCenter, QStringLiteral("1"));
  } else if (action == QStringLiteral("tool-rectangle")) {
    painter.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
  } else if (action == QStringLiteral("tool-text")) {
    painter.drawLine(QPointF(5, 5), QPointF(19, 5));
    painter.drawLine(QPointF(12, 5), QPointF(12, 19));
    painter.drawLine(QPointF(9, 19), QPointF(15, 19));
  } else if (action == QStringLiteral("ocr")) {
    QPainterPath path;
    path.moveTo(9, 4);
    path.lineTo(5, 4);
    path.lineTo(5, 8);
    path.moveTo(15, 4);
    path.lineTo(19, 4);
    path.lineTo(19, 8);
    path.moveTo(9, 20);
    path.lineTo(5, 20);
    path.lineTo(5, 16);
    path.moveTo(15, 20);
    path.lineTo(19, 20);
    path.lineTo(19, 16);
    path.moveTo(8, 9);
    path.lineTo(16, 9);
    path.moveTo(8, 13);
    path.lineTo(16, 13);
    path.moveTo(8, 17);
    path.lineTo(13, 17);
    painter.drawPath(path);
  } else if (action == QStringLiteral("background")) {
    painter.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2);
    painter.drawEllipse(QPointF(8, 9), 1.5, 1.5);
    QPainterPath path;
    path.moveTo(3, 17);
    path.lineTo(8, 12);
    path.lineTo(11, 15);
    path.lineTo(14, 12);
    path.lineTo(21, 19);
    painter.drawPath(path);
  } else if (action == QStringLiteral("undo")) {
    QPainterPath path;
    path.moveTo(9, 7);
    path.lineTo(4, 12);
    path.lineTo(9, 17);
    path.moveTo(5, 12);
    path.cubicTo(8, 8, 14, 7, 18, 10);
    path.cubicTo(20, 12, 20, 15, 19, 17);
    painter.drawPath(path);
  } else if (action == QStringLiteral("copy") || action == QStringLiteral("both")) {
    painter.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
    QPainterPath path;
    path.moveTo(16, 8);
    path.lineTo(16, 6);
    path.quadTo(16, 4, 14, 4);
    path.lineTo(6, 4);
    path.quadTo(4, 4, 4, 6);
    path.lineTo(4, 14);
    path.quadTo(4, 16, 6, 16);
    path.lineTo(8, 16);
    painter.drawPath(path);
    if (action == QStringLiteral("both")) {
      painter.setBrush(QColor(QStringLiteral("#0a84ff")));
      painter.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.drawEllipse(QPointF(18, 18), 4.5, 4.5);
      painter.drawLine(QPointF(16, 18), QPointF(17.5, 19.5));
      painter.drawLine(QPointF(17.5, 19.5), QPointF(20.5, 16.5));
    }
  } else if (action == QStringLiteral("save")) {
    painter.drawLine(QPointF(12, 4), QPointF(12, 15));
    painter.drawLine(QPointF(8, 11), QPointF(12, 15));
    painter.drawLine(QPointF(12, 15), QPointF(16, 11));
    QPainterPath tray;
    tray.moveTo(5, 18);
    tray.lineTo(5, 20);
    tray.lineTo(19, 20);
    tray.lineTo(19, 18);
    painter.drawPath(tray);
  } else if (action == QStringLiteral("close")) {
    painter.drawLine(QPointF(6, 6), QPointF(18, 18));
    painter.drawLine(QPointF(18, 6), QPointF(6, 18));
  }
  painter.restore();
}

void drawInstantTooltip(QPainter &painter, const QRect &bounds, const QRectF &anchor,
                        const QString &text) {
  if (text.isEmpty())
    return;
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setPixelSize(12);
  painter.setFont(font);
  const qreal width = painter.fontMetrics().horizontalAdvance(text) + 20;
  const qreal height = 28;
  qreal x = std::clamp(anchor.center().x() - width / 2.0, 8.0,
                       std::max(8.0, bounds.width() - width - 8.0));
  qreal y = anchor.top() - height - 7;
  if (y < 6)
    y = anchor.bottom() + 7;
  const QRectF pill(x, y, width, height);
  painter.setPen(QPen(QColor(255, 255, 255, 42), 1));
  painter.setBrush(QColor(12, 12, 15, 248));
  painter.drawRoundedRect(pill, 7, 7);
  painter.setPen(Qt::white);
  painter.drawText(pill, Qt::AlignCenter, text);
}
} // namespace

CaptureEditor::CaptureEditor(CaptureData capture, QWidget *parent)
    : QWidget(parent), capture_(std::move(capture)), fullscreenPreview_(capture_.preview) {
  setWindowTitle(QStringLiteral("Omarchy Capture Editor"));
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen->name() == capture_.monitor.name) {
      setGeometry(screen->geometry());
      break;
    }
  }
  if (geometry().isEmpty())
    setGeometry(QGuiApplication::primaryScreen()->geometry());
  cursor_ = mapFromGlobal(QCursor::pos());

  textEditor_ = new QLineEdit(this);
  textEditor_->hide();
  textEditor_->setFrame(false);
  textEditor_->setStyleSheet(QStringLiteral(
      "QLineEdit { color: #ff375f; background: transparent; border: none; padding: 0;"
      " selection-background-color: #0a84ff; }"));
  textEditor_->installEventFilter(this);
  connect(textEditor_, &QLineEdit::textChanged, this, [this](const QString &text) {
    const QFontMetrics metrics(textEditor_->font());
    const int desiredWidth =
        std::max(48, metrics.horizontalAdvance(text + QStringLiteral("  ")));
    const int availableWidth =
        std::max(48, qRound(editImageRect().right() - textEditor_->x()));
    textEditor_->resize(std::min(desiredWidth, availableWidth), textEditor_->height());
  });

  connect(&ocrWatcher_, &QFutureWatcher<OcrResult>::finished, this, [this] {
    const OcrResult result = ocrWatcher_.result();
    busy_ = false;
    if (!result.error.isEmpty()) {
      setStatus(result.error);
      return;
    }
    QString clipboardError;
    if (!copyTextToClipboard(result.text, clipboardError)) {
      setStatus(clipboardError);
      return;
    }
    setStatus(QStringLiteral("OCR copied to clipboard"));
    sendCaptureNotification(QStringLiteral("Copied text from screenshot"));
  });
}

bool CaptureEditor::eventFilter(QObject *watched, QEvent *event) {
  if (watched == textEditor_ && event->type() == QEvent::KeyPress) {
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      acceptText();
      return true;
    }
    if (key->key() == Qt::Key_Escape) {
      textEditor_->clear();
      textEditor_->hide();
      setFocus(Qt::OtherFocusReason);
      update();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

QColor CaptureEditor::annotationColor() const {
  return QColor(QString::fromLatin1(kColorNames.at(static_cast<std::size_t>(colorIndex_))));
}

QRectF CaptureEditor::normalizedSelection(const QPointF &first, const QPointF &second) const {
  const QRectF bounds(QPointF(), QSizeF(width(), height()));
  const QPointF a(std::clamp(first.x(), bounds.left(), bounds.right()),
                  std::clamp(first.y(), bounds.top(), bounds.bottom()));
  const QPointF b(std::clamp(second.x(), bounds.left(), bounds.right()),
                  std::clamp(second.y(), bounds.top(), bounds.bottom()));
  return QRectF(a, b).normalized();
}

QRectF CaptureEditor::editImageRect() const {
  if (selection_.isEmpty())
    return {};
  const QRectF available(30, 68, std::max(1, width() - 60), std::max(1, height() - 126));
  const qreal scale = std::min<qreal>({1.0, available.width() / selection_.width(),
                                      available.height() / selection_.height()});
  const QSizeF shown = selection_.size() * scale;
  return {available.center().x() - shown.width() / 2.0,
          available.center().y() - shown.height() / 2.0, shown.width(), shown.height()};
}

qreal CaptureEditor::editScale() const {
  return selection_.width() > 0 ? editImageRect().width() / selection_.width() : 1.0;
}

QPointF CaptureEditor::toAnnotationPoint(const QPointF &position) const {
  const QRectF image = editImageRect();
  const qreal scale = std::max<qreal>(editScale(), 0.001);
  return {std::clamp((position.x() - image.left()) / scale, 0.0, selection_.width()),
          std::clamp((position.y() - image.top()) / scale, 0.0, selection_.height())};
}

int CaptureEditor::windowAt(const QPointF &position) const {
  for (int index = capture_.windows.size() - 1; index >= 0; --index) {
    if (capture_.windows.at(index).rect.contains(position.toPoint()))
      return index;
  }
  return -1;
}

int CaptureEditor::windowInDirection(int current, int key) const {
  if (capture_.windows.isEmpty())
    return -1;

  const QPointF origin =
      current >= 0 && current < capture_.windows.size()
          ? capture_.windows.at(current).rect.center()
          : cursor_;
  int best = -1;
  qreal bestScore = std::numeric_limits<qreal>::max();
  for (int index = 0; index < capture_.windows.size(); ++index) {
    if (index == current)
      continue;
    const QPointF delta = capture_.windows.at(index).rect.center() - origin;
    qreal along = 0;
    qreal across = 0;
    if (key == Qt::Key_Left && delta.x() < 0) {
      along = -delta.x();
      across = std::abs(delta.y());
    } else if (key == Qt::Key_Right && delta.x() > 0) {
      along = delta.x();
      across = std::abs(delta.y());
    } else if (key == Qt::Key_Up && delta.y() < 0) {
      along = -delta.y();
      across = std::abs(delta.x());
    } else if (key == Qt::Key_Down && delta.y() > 0) {
      along = delta.y();
      across = std::abs(delta.x());
    } else {
      continue;
    }
    const qreal score = along + across * 1.75;
    if (score < bestScore) {
      best = index;
      bestScore = score;
    }
  }
  return best;
}

QVector<CaptureEditor::ToolbarButton> CaptureEditor::toolbarButtons() const {
  QVector<ToolbarButton> buttons;
  const qreal height = 36;
  const qreal gap = 4;
  const qreal total = 4 * (36 + gap) + 6 * (24 + gap) + (42 + gap) +
                      5 * (36 + gap) + (40 + gap) + 36;
  qreal x = (width() - total) / 2.0;
  const qreal y = std::max<qreal>(10, editImageRect().top() - height - 10);
  auto add = [&](qreal buttonWidth, QString action, QString label, QString tooltip,
                 QColor color = {}) {
    buttons.push_back({QRectF(x, y, buttonWidth, height), std::move(action),
                       std::move(label), std::move(tooltip), color});
    x += buttonWidth + gap;
  };

  add(36, QStringLiteral("tool-arrow"), {}, QStringLiteral("Arrow · A"));
  add(36, QStringLiteral("tool-marker"), {}, QStringLiteral("Number marker · C"));
  add(36, QStringLiteral("tool-rectangle"), {}, QStringLiteral("Rectangle · R"));
  add(36, QStringLiteral("tool-text"), {}, QStringLiteral("Text · T"));
  for (int index = 0; index < 6; ++index) {
    add(24, QStringLiteral("color-%1").arg(index), {},
        QStringLiteral("Color · %1").arg(index + 1),
        QColor(QString::fromLatin1(kColorNames.at(static_cast<std::size_t>(index)))));
  }
  add(42, QStringLiteral("size"), QString::number(qRound(annotationSize_)),
      QStringLiteral("Annotation size · Wheel"));
  add(36, QStringLiteral("ocr"), {}, QStringLiteral("Copy text with OCR · O"));
  add(36, QStringLiteral("background"), {}, QStringLiteral("Backdrop and shadow · B"));
  add(36, QStringLiteral("undo"), {}, QStringLiteral("Undo · Ctrl+Z"));
  add(36, QStringLiteral("copy"), {}, QStringLiteral("Copy · Enter"));
  add(40, QStringLiteral("both"), {}, QStringLiteral("Copy and save · Shift+Enter"));
  add(36, QStringLiteral("save"), {}, QStringLiteral("Save · Ctrl+S"));
  add(36, QStringLiteral("close"), {}, QStringLiteral("Close · Esc"));
  return buttons;
}

void CaptureEditor::setStatus(QString status) {
  status_ = std::move(status);
  update();
}

void CaptureEditor::chooseWindow(int index) {
  if (index < 0 || index >= capture_.windows.size())
    return;

  const WindowTarget target = capture_.windows.at(index);
  setStatus(QStringLiteral("Capturing clean window surface…"));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QImage surface;
  QString surfaceError;
  if (captureWindowSurface(target, surface, surfaceError)) {
    capture_.source = surface;
    capture_.preview =
        surface.scaled(target.rect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    selection_ = QRectF(QPointF(), target.rect.size());
    setStatus(QStringLiteral("Window surface selected · annotate, OCR, copy or save"));
  } else {
    selection_ = target.rect;
    setStatus(QStringLiteral("Window crop selected · %1").arg(surfaceError));
  }
  phase_ = Phase::Edit;
  windowMode_ = false;
  updatePointerCursor();
}

void CaptureEditor::beginText(const QPointF &point) {
  textPoint_ = point;
  textColor_ = annotationColor();
  textSize_ = annotationSize_;
  const QRectF image = editImageRect();
  const qreal scale = editScale();
  const QPointF position = image.topLeft() + point * scale;
  QFont displayFont = annotationTextFont(textSize_);
  displayFont.setPixelSize(std::max(12, qRound(displayFont.pixelSize() * scale)));
  textEditor_->setFont(displayFont);
  textEditor_->setStyleSheet(
      QStringLiteral("QLineEdit { color: %1; background: transparent; border: none; padding: 0;"
                     " selection-background-color: #0a84ff; }")
          .arg(textColor_.name()));
  textEditor_->setGeometry(qRound(position.x()), qRound(position.y()), 72,
                           QFontMetrics(displayFont).height() + 4);
  textEditor_->clear();
  textEditor_->show();
  textEditor_->raise();
  textEditor_->setFocus(Qt::MouseFocusReason);
}

void CaptureEditor::acceptText() {
  const QString text = textEditor_->text().trimmed();
  if (!text.isEmpty()) {
    Annotation annotation;
    annotation.kind = Annotation::Kind::Text;
    annotation.start =
        textPoint_ + QPointF(0, QFontMetricsF(annotationTextFont(textSize_)).ascent());
    annotation.text = text;
    annotation.color = textColor_;
    annotation.size = textSize_;
    annotations_.push_back(std::move(annotation));
  }
  textEditor_->clear();
  textEditor_->hide();
  setFocus(Qt::OtherFocusReason);
  update();
}

void CaptureEditor::selectWindowInDirection(int key) {
  int current = hoveredWindow_;
  if (current < 0)
    current = windowAt(cursor_);
  const int next = windowInDirection(current, key);
  if (next < 0)
    return;
  hoveredWindow_ = next;
  setStatus(QStringLiteral("%1 · Super+Arrows choose · Enter captures")
                .arg(capture_.windows.at(next).title));
}

void CaptureEditor::runOcr() {
  if (busy_ || selection_.isEmpty())
    return;
  busy_ = true;
  setStatus(QStringLiteral("Reading text…"));
  const QImage image = renderCapture(capture_, selection_, {}, false);
  ocrWatcher_.setFuture(QtConcurrent::run([image] {
    OcrResult result;
    result.text = recognizeText(image, result.error);
    return result;
  }));
}

void CaptureEditor::finish(OutputMode mode) {
  if (busy_ || selection_.isEmpty())
    return;
  busy_ = true;
  setStatus(QStringLiteral("Rendering screenshot…"));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  const QImage image = renderCapture(capture_, selection_, annotations_, backgroundEnabled_);
  if (image.isNull()) {
    busy_ = false;
    setStatus(QStringLiteral("Could not render screenshot"));
    return;
  }

  QString error;
  QString saved;
  if (mode == OutputMode::Copy || mode == OutputMode::Both) {
    if (!copyPngToClipboard(image, error)) {
      busy_ = false;
      setStatus(error);
      return;
    }
  }
  if (mode == OutputMode::Save || mode == OutputMode::Both) {
    saved = saveScreenshot(image, error);
    if (saved.isEmpty()) {
      busy_ = false;
      setStatus(error);
      return;
    }
  }

  if (mode == OutputMode::Copy)
    sendCaptureNotification(QStringLiteral("Screenshot copied to clipboard"));
  else if (mode == OutputMode::Save)
    sendCaptureNotification(QStringLiteral("Screenshot saved"), saved);
  else
    sendCaptureNotification(QStringLiteral("Screenshot saved and copied"), saved);
  close();
}

void CaptureEditor::handleToolbar(const QString &action) {
  if (action == QStringLiteral("tool-arrow"))
    tool_ = Tool::Arrow;
  else if (action == QStringLiteral("tool-marker"))
    tool_ = Tool::Marker;
  else if (action == QStringLiteral("tool-rectangle"))
    tool_ = Tool::Rectangle;
  else if (action == QStringLiteral("tool-text"))
    tool_ = Tool::Text;
  else if (action.startsWith(QStringLiteral("color-")))
    colorIndex_ = std::clamp(action.sliced(6).toInt(), 0, 5);
  else if (action == QStringLiteral("ocr"))
    runOcr();
  else if (action == QStringLiteral("background"))
    backgroundEnabled_ = !backgroundEnabled_;
  else if (action == QStringLiteral("undo")) {
    if (!annotations_.isEmpty())
      annotations_.removeLast();
  } else if (action == QStringLiteral("copy"))
    finish(OutputMode::Copy);
  else if (action == QStringLiteral("both"))
    finish(OutputMode::Both);
  else if (action == QStringLiteral("save"))
    finish(OutputMode::Save);
  else if (action == QStringLiteral("close"))
    close();
  updatePointerCursor();
  update();
}

void CaptureEditor::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  if (phase_ == Phase::Select) {
    const bool directionalKey = event->key() == Qt::Key_Left ||
                                event->key() == Qt::Key_Right ||
                                event->key() == Qt::Key_Up ||
                                event->key() == Qt::Key_Down;
    if (windowMode_ && directionalKey &&
        event->modifiers().testFlag(Qt::MetaModifier)) {
      selectWindowInDirection(event->key());
      event->accept();
      update();
      return;
    }
    if (windowMode_ &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
      chooseWindow(hoveredWindow_);
      return;
    }
    if (event->key() == Qt::Key_Space) {
      windowMode_ = !windowMode_;
      dragging_ = false;
      selection_ = {};
      hoveredWindow_ = windowMode_ ? windowAt(cursor_) : -1;
      setStatus(
          windowMode_
              ? QStringLiteral("Window mode · click or Super+Arrows then Enter · Space returns to area")
              : QStringLiteral("Drag to select an area · Space selects a window"));
      updatePointerCursor();
      return;
    }
    QWidget::keyPressEvent(event);
    return;
  }

  if (event->matches(QKeySequence::Undo)) {
    if (!annotations_.isEmpty())
      annotations_.removeLast();
  } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
             event->modifiers().testFlag(Qt::ShiftModifier)) {
    finish(OutputMode::Both);
    return;
  } else if (event->matches(QKeySequence::Save)) {
    finish(OutputMode::Save);
    return;
  } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    finish(OutputMode::Copy);
    return;
  } else if (event->key() == Qt::Key_A) {
    tool_ = Tool::Arrow;
  } else if (event->key() == Qt::Key_C || event->key() == Qt::Key_M) {
    tool_ = Tool::Marker;
  } else if (event->key() == Qt::Key_R) {
    tool_ = Tool::Rectangle;
  } else if (event->key() == Qt::Key_T) {
    tool_ = Tool::Text;
  } else if (event->key() == Qt::Key_O) {
    runOcr();
  } else if (event->key() == Qt::Key_B) {
    backgroundEnabled_ = !backgroundEnabled_;
  } else if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6) {
    colorIndex_ = event->key() - Qt::Key_1;
  } else {
    QWidget::keyPressEvent(event);
    return;
  }
  updatePointerCursor();
  update();
}

void CaptureEditor::mouseMoveEvent(QMouseEvent *event) {
  cursor_ = event->position();
  if (phase_ == Phase::Select) {
    if (windowMode_)
      hoveredWindow_ = windowAt(cursor_);
    else if (dragging_)
      selection_ = normalizedSelection(dragStart_, cursor_);
  }
  updatePointerCursor();
  update();
}

void CaptureEditor::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton || busy_)
    return;
  cursor_ = event->position();
  if (phase_ == Phase::Select) {
    if (windowMode_) {
      chooseWindow(windowAt(cursor_));
      return;
    }
    dragStart_ = cursor_;
    selection_ = {};
    dragging_ = true;
    return;
  }

  for (const ToolbarButton &button : toolbarButtons()) {
    if (button.rect.contains(cursor_)) {
      handleToolbar(button.action);
      return;
    }
  }
  if (!editImageRect().contains(cursor_))
    return;

  const QPointF point = toAnnotationPoint(cursor_);
  if (tool_ == Tool::Marker) {
    Annotation annotation;
    annotation.kind = Annotation::Kind::Marker;
    annotation.start = point;
    annotation.number = nextMarker_++;
    annotation.color = annotationColor();
    annotation.size = annotationSize_;
    annotations_.push_back(std::move(annotation));
  } else if (tool_ == Tool::Text) {
    beginText(point);
  } else {
    dragStart_ = point;
    dragging_ = true;
  }
  update();
}

void CaptureEditor::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton || !dragging_)
    return;
  if (phase_ == Phase::Select) {
    selection_ = normalizedSelection(dragStart_, event->position());
    dragging_ = false;
    if (selection_.width() >= 2 && selection_.height() >= 2) {
      phase_ = Phase::Edit;
      setStatus(QStringLiteral("Area selected · annotate, OCR, copy or save"));
    }
    updatePointerCursor();
    update();
    return;
  }

  const QPointF end = toAnnotationPoint(event->position());
  if (QLineF(dragStart_, end).length() > 4) {
    Annotation annotation;
    annotation.kind = tool_ == Tool::Rectangle ? Annotation::Kind::Rectangle
                                                : Annotation::Kind::Arrow;
    annotation.start = dragStart_;
    annotation.end = end;
    annotation.color = annotationColor();
    annotation.size = annotationSize_;
    annotations_.push_back(std::move(annotation));
  }
  dragging_ = false;
  update();
}

void CaptureEditor::wheelEvent(QWheelEvent *event) {
  if (phase_ != Phase::Edit) {
    QWidget::wheelEvent(event);
    return;
  }
  const int step = event->angleDelta().y() > 0 ? 1 : -1;
  annotationSize_ = std::clamp(annotationSize_ + step, 2.0, 12.0);
  setStatus(QStringLiteral("Size %1 · mouse wheel changes size").arg(qRound(annotationSize_)));
  event->accept();
}

void CaptureEditor::updatePointerCursor() {
  if (phase_ == Phase::Select) {
    setCursor(windowMode_ ? Qt::PointingHandCursor : Qt::CrossCursor);
    return;
  }
  for (const ToolbarButton &button : toolbarButtons()) {
    if (button.rect.contains(cursor_)) {
      setCursor(Qt::PointingHandCursor);
      return;
    }
  }
  if (tool_ == Tool::Marker)
    setCursor(Qt::PointingHandCursor);
  else if (tool_ == Tool::Text)
    setCursor(Qt::IBeamCursor);
  else
    setCursor(Qt::CrossCursor);
}

void CaptureEditor::paintSelect(QPainter &painter) {
  painter.drawImage(rect(), capture_.preview);
  painter.fillRect(rect(), QColor(0, 0, 0, 143));

  if (windowMode_) {
    if (hoveredWindow_ >= 0 && hoveredWindow_ < capture_.windows.size()) {
      const QRect window = capture_.windows.at(hoveredWindow_).rect;
      painter.drawImage(window, capture_.preview, window);
    }
    for (int index = 0; index < capture_.windows.size(); ++index) {
      const WindowTarget &window = capture_.windows.at(index);
      painter.setPen(QPen(index == hoveredWindow_ ? Qt::white : QColor(255, 255, 255, 72), 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(window.rect);
    }
  } else if (!selection_.isEmpty()) {
    painter.drawImage(selection_, capture_.preview, selection_);
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selection_);
  }

  if (!windowMode_ && !dragging_) {
    painter.setPen(QPen(QColor(255, 255, 255, 56), 1));
    painter.drawLine(QPointF(cursor_.x(), 0), QPointF(cursor_.x(), height()));
    painter.drawLine(QPointF(0, cursor_.y()), QPointF(width(), cursor_.y()));
  }

  QFont badgeFont(QStringLiteral("Noto Sans"));
  badgeFont.setBold(true);
  badgeFont.setPixelSize(11);
  painter.setFont(badgeFont);
  const QString badge = windowMode_ ? QStringLiteral("WINDOW  ×") : QStringLiteral("AREA  ×");
  const int badgeWidth = painter.fontMetrics().horizontalAdvance(badge) + 24;
  const QRectF badgeRect((width() - badgeWidth) / 2.0, 12, badgeWidth, 32);
  painter.setPen(QPen(QColor(255, 255, 255, 32), 1));
  painter.setBrush(QColor(18, 18, 22, 235));
  painter.drawRoundedRect(badgeRect, 10, 10);
  painter.setPen(windowMode_ ? QColor(QStringLiteral("#ffd60a"))
                             : QColor(QStringLiteral("#30d158")));
  painter.drawText(badgeRect, Qt::AlignCenter, badge);
  drawStatusPill(painter, rect(), status_);
}

void CaptureEditor::paintEdit(QPainter &painter) {
  painter.drawImage(rect(), fullscreenPreview_);
  painter.fillRect(rect(), QColor(0, 0, 0, 160));
  const QRectF image = editImageRect();
  if (backgroundEnabled_) {
    const QRectF backing = image.adjusted(-22, -22, 22, 22);
    QLinearGradient gradient(backing.topLeft(), backing.bottomRight());
    gradient.setColorAt(0, QColor(QStringLiteral("#1d2030")));
    gradient.setColorAt(1, QColor(QStringLiteral("#364f78")));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 105));
    painter.drawRoundedRect(backing.translated(0, 10), 18, 18);
    painter.setBrush(gradient);
    painter.drawRoundedRect(backing, 16, 16);
  }

  QPainterPath clip;
  clip.addRoundedRect(image, backgroundEnabled_ ? 10 : 0, backgroundEnabled_ ? 10 : 0);
  painter.save();
  painter.setClipPath(clip);
  painter.drawImage(image, capture_.preview, selection_);
  painter.restore();

  painter.save();
  painter.translate(image.topLeft());
  painter.scale(editScale(), editScale());
  for (const Annotation &annotation : annotations_)
    paintAnnotation(painter, annotation);
  if (dragging_) {
    Annotation preview;
    preview.kind = tool_ == Tool::Rectangle ? Annotation::Kind::Rectangle
                                            : Annotation::Kind::Arrow;
    preview.start = dragStart_;
    preview.end = toAnnotationPoint(cursor_);
    preview.color = annotationColor();
    preview.size = annotationSize_;
    paintAnnotation(painter, preview);
  } else if (tool_ == Tool::Marker && image.contains(cursor_)) {
    Annotation preview;
    preview.kind = Annotation::Kind::Marker;
    preview.start = toAnnotationPoint(cursor_);
    preview.number = nextMarker_;
    preview.color = annotationColor();
    preview.color.setAlpha(185);
    preview.size = annotationSize_;
    paintAnnotation(painter, preview);
  }
  painter.restore();

  const QString currentTool = toolAction(tool_);
  QFont buttonFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  buttonFont.setPixelSize(11);
  buttonFont.setBold(true);
  painter.setFont(buttonFont);
  const QVector<ToolbarButton> buttons = toolbarButtons();
  const ToolbarButton *hoveredButton = nullptr;
  for (const ToolbarButton &button : buttons) {
    const bool selected = button.action == currentTool ||
                          (button.action == QStringLiteral("background") && backgroundEnabled_) ||
                          (button.action == QStringLiteral("color-%1").arg(colorIndex_));
    const bool hovered = button.rect.contains(cursor_);
    if (hovered)
      hoveredButton = &button;
    painter.setPen(QPen(QColor(255, 255, 255, selected ? 64 : 26), 1));
    painter.setBrush(selected ? QColor(66, 66, 75, 250)
                              : (hovered ? QColor(48, 48, 56, 248) : QColor(34, 34, 40, 244)));
    if (button.action == QStringLiteral("both"))
      painter.setBrush(QColor(QStringLiteral("#0a84ff")));
    painter.drawRoundedRect(button.rect, 8, 8);
    if (button.color.isValid()) {
      const QPointF center = button.rect.center();
      painter.setPen(QPen(selected ? Qt::white : QColor(255, 255, 255, 80), selected ? 2 : 1));
      painter.setBrush(button.color);
      painter.drawEllipse(center, 7, 7);
    } else {
      drawToolbarIcon(painter, button.rect, button.action, button.label,
                      QColor(245, 245, 247));
    }
  }
  drawStatusPill(painter, rect(), status_);
  if (hoveredButton)
    drawInstantTooltip(painter, rect(), hoveredButton->rect, hoveredButton->tooltip);
}

void CaptureEditor::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                         QPainter::TextAntialiasing);
  if (phase_ == Phase::Select)
    paintSelect(painter);
  else
    paintEdit(painter);
}
