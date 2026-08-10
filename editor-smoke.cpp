#include "capture.hpp"
#include "editor.hpp"

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QtTest/QTest>

int main(int argc, char **argv) {
  QApplication application(argc, argv);
  const QString outputRoot =
      argc > 1 ? QString::fromLocal8Bit(argv[1])
               : QDir(QDir::tempPath()).filePath(QStringLiteral("omarchy-native-smoke"));

  CaptureData capture;
  capture.monitor.name = QStringLiteral("TEST");
  capture.monitor.geometry = {0, 0, 800, 600};
  capture.monitor.pixelSize = {800, 600};
  capture.monitor.workspaceId = 42;
  capture.source = QImage(800, 600, QImage::Format_ARGB32_Premultiplied);
  {
    QPainter painter(&capture.source);
    QLinearGradient gradient(0, 0, 800, 600);
    gradient.setColorAt(0, QColor(QStringLiteral("#172033")));
    gradient.setColorAt(1, QColor(QStringLiteral("#5278b5")));
    painter.fillRect(capture.source.rect(), gradient);
    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Noto Sans"), 34, QFont::Bold));
    painter.drawText(capture.source.rect(), Qt::AlignCenter,
                     QStringLiteral("Native Qt capture editor"));
  }
  capture.preview = capture.source;
  capture.windows = {{{80, 80, 300, 220}, QStringLiteral("first")},
                     {{420, 120, 300, 320}, QStringLiteral("second")}};

  CaptureEditor editor(capture);
  editor.resize(800, 600);
  editor.show();
  application.processEvents();
  QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
  QTest::mouseMove(&editor, QPoint(650, 470), 20);
  QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(650, 470));
  QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(230, 250));
  QTest::mouseMove(&editor, QPoint(570, 350), 20);
  QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(570, 350));
  QTest::keyClick(&editor, Qt::Key_2);
  QTest::keyClick(&editor, Qt::Key_C);
  QTest::mouseClick(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(470, 300));
  QTest::keyClick(&editor, Qt::Key_B);
  application.processEvents();
  if (!editor.grab().save(outputRoot + QStringLiteral("-ui.png"), "PNG"))
    return 2;

  QVector<Annotation> annotations;
  annotations.push_back({Annotation::Kind::Arrow, {50, 60}, {390, 150}, {},
                         QColor(QStringLiteral("#ff375f")), 5, 0});
  annotations.push_back({Annotation::Kind::Marker, {260, 180}, {}, {},
                         QColor(QStringLiteral("#ff9f0a")), 5, 1});
  annotations.push_back({Annotation::Kind::Rectangle, {30, 30}, {450, 230}, {},
                         QColor(QStringLiteral("#30d158")), 4, 0});
  annotations.push_back({Annotation::Kind::Text, {80, 210}, {},
                         QStringLiteral("Qt C++"), QColor(QStringLiteral("#0a84ff")), 4, 0});
  const QImage rendered = renderCapture(capture, QRectF(100, 100, 500, 300), annotations, true);
  if (rendered.isNull() || rendered.size() != QSize(596, 396) ||
      !rendered.save(outputRoot + QStringLiteral("-render.png"), "PNG"))
    return 3;

  if (qEnvironmentVariableIsSet("OMARCHY_CAPTURE_SMOKE_COPY")) {
    QString clipboardError;
    if (!copyPngToClipboard(rendered, clipboardError))
      return 4;
  }

  if (qEnvironmentVariableIsSet("OMARCHY_CAPTURE_SMOKE_SAVE")) {
    QString saveError;
    if (saveScreenshot(rendered, saveError).isEmpty())
      return 6;
  }

  QImage ocrImage(1000, 260, QImage::Format_RGB32);
  ocrImage.fill(Qt::white);
  {
    QPainter painter(&ocrImage);
    painter.setPen(Qt::black);
    painter.setFont(QFont(QStringLiteral("Noto Sans"), 64, QFont::Bold));
    painter.drawText(ocrImage.rect(), Qt::AlignCenter, QStringLiteral("OCR smoke test 42"));
  }
  QString ocrError;
  if (!recognizeText(ocrImage, ocrError).contains(QStringLiteral("OCR smoke test 42")))
    return 5;
  return 0;
}
