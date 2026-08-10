#include "capture.hpp"
#include "editor.hpp"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QScreen>
#include <QStandardPaths>
#include <QWindow>

int main(int argc, char **argv) {
  QCoreApplication::setApplicationName(QStringLiteral("omarchy-capture-editor"));
  QCoreApplication::setOrganizationName(QStringLiteral("Omarchy"));
  QGuiApplication::setDesktopFileName(QStringLiteral("omarchy-capture-editor"));
  QApplication application(argc, argv);
  application.setQuitOnLastWindowClosed(true);

  QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty())
    runtime = QDir::tempPath();
  QLockFile instanceLock(QDir(runtime).filePath(QStringLiteral("omarchy-capture-editor.instance")));
  instanceLock.setStaleLockTime(0);
  if (!instanceLock.tryLock(0))
    return 0;

  CaptureData capture;
  QString error;
  if (!captureFocusedMonitor(capture, error)) {
    qCritical().noquote() << error;
    sendCaptureNotification(QStringLiteral("Screenshot failed: %1").arg(error));
    return 1;
  }

  qInfo().noquote() << QStringLiteral("Captured %1 workspace %2 with %3 selectable windows")
                           .arg(capture.monitor.name)
                           .arg(capture.monitor.workspaceId)
                           .arg(capture.windows.size());

  QScreen *targetScreen = QGuiApplication::primaryScreen();
  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen->name() == capture.monitor.name) {
      targetScreen = screen;
      break;
    }
  }

  CaptureEditor editor(std::move(capture));
  editor.setScreen(targetScreen);
  editor.setGeometry(targetScreen->geometry());
  editor.showFullScreen();
  editor.raise();
  editor.activateWindow();
  editor.setFocus(Qt::ActiveWindowFocusReason);
  return application.exec();
}
