#include "capture.hpp"
#include "editor.hpp"
#include <LayerShellQt/Window>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

int main(int argc, char **argv) {
  QCoreApplication::setApplicationName(
      QStringLiteral("omarchy-capture-editor"));
  QCoreApplication::setApplicationVersion(
      QString::fromLatin1(OMARCHY_CAPTURE_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("Omarchy"));
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
  QGuiApplication::setDesktopFileName(QStringLiteral("omarchy-capture-editor"));
  QApplication application(argc, argv);

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Native Wayland screenshot and annotation overlay for "
                     "Hyprland and Omarchy."));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption fullscreenOption(
      QStringLiteral("capture-fullscreen"),
      QStringLiteral("Start with the entire focused monitor selected."));
  const QCommandLineOption windowOption(
      {QStringLiteral("capture-window"), QStringLiteral("capture-windows")},
      QStringLiteral("Start in window selection mode."));
  const QCommandLineOption regionOption(
      QStringLiteral("capture-region"),
      QStringLiteral("Start in freeform region selection mode (default)."));
  parser.addOption(fullscreenOption);
  parser.addOption(windowOption);
  parser.addOption(regionOption);
  parser.addPositionalArgument(
      QStringLiteral("mode"),
      QStringLiteral("Optional compatibility mode: smart, region, windows, or "
                     "fullscreen."),
      QStringLiteral("[mode]"));
  parser.process(application);

  CaptureEditor::CaptureMode captureMode = CaptureEditor::CaptureMode::Region;
  int requestedModes = parser.isSet(fullscreenOption) +
                       parser.isSet(windowOption) + parser.isSet(regionOption);
  if (parser.isSet(fullscreenOption))
    captureMode = CaptureEditor::CaptureMode::Fullscreen;
  else if (parser.isSet(windowOption))
    captureMode = CaptureEditor::CaptureMode::Window;

  const QStringList positional = parser.positionalArguments();
  if (positional.size() > 1) {
    qCritical() << "Only one capture mode may be specified";
    return 2;
  }
  if (!positional.isEmpty()) {
    ++requestedModes;
    const QString mode = positional.first();
    if (mode == QStringLiteral("fullscreen"))
      captureMode = CaptureEditor::CaptureMode::Fullscreen;
    else if (mode == QStringLiteral("windows") ||
             mode == QStringLiteral("window"))
      captureMode = CaptureEditor::CaptureMode::Window;
    else if (mode == QStringLiteral("smart") ||
             mode == QStringLiteral("region"))
      captureMode = CaptureEditor::CaptureMode::Region;
    else {
      qCritical().noquote()
          << QStringLiteral("Unknown capture mode: %1").arg(mode);
      return 2;
    }
  }
  if (requestedModes > 1) {
    qCritical() << "Capture mode options are mutually exclusive";
    return 2;
  }
  if (!loadCaptureFonts())
    return 1;
  application.setQuitOnLastWindowClosed(true);

  QString runtime =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty())
    runtime = QDir::tempPath();
  QLockFile instanceLock(QDir(runtime).filePath(
      QStringLiteral("omarchy-capture-editor.instance")));
  instanceLock.setStaleLockTime(0);
  if (!instanceLock.tryLock(0))
    return 0;

  CaptureData capture;
  QString error;
  QProcess freeze;
  if (!captureFocusedMonitor(capture, error, &freeze)) {
    qCritical().noquote() << error;
    sendCaptureNotification(QStringLiteral("Screenshot failed: %1").arg(error));
    return 1;
  }

  qInfo().noquote()
      << QStringLiteral("Captured %1 workspace %2 with %3 selectable windows")
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

  CaptureEditor editor(std::move(capture), captureMode);
  editor.setScreen(targetScreen);
  editor.setGeometry(targetScreen->geometry());
  editor.winId();
  QWindow *window = editor.windowHandle();
  LayerShellQt::Window *layerWindow = LayerShellQt::Window::get(window);
  if (!window || !layerWindow) {
    stopCaptureFreeze(freeze);
    qCritical() << "Could not create capture overlay layer";
    return 1;
  }
  layerWindow->setScope(QStringLiteral("omarchy-capture-editor"));
  layerWindow->setScreen(targetScreen);
  layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
  LayerShellQt::Window::Anchors anchors;
  anchors.setFlag(LayerShellQt::Window::AnchorTop);
  anchors.setFlag(LayerShellQt::Window::AnchorBottom);
  anchors.setFlag(LayerShellQt::Window::AnchorLeft);
  anchors.setFlag(LayerShellQt::Window::AnchorRight);
  layerWindow->setAnchors(anchors);
  layerWindow->setExclusiveZone(-1);
  layerWindow->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityExclusive);
  layerWindow->setActivateOnShow(true);
  editor.show();
  editor.setFocus(Qt::ActiveWindowFocusReason);

  // Keep hyprpicker's frozen overlay alive until this layer has been committed
  // for several refreshes. The two identical frames overlap; the workspace is
  // never exposed between capture and editor mapping.
  QTimer::singleShot(50, &application,
                     [&freeze] { stopCaptureFreeze(freeze); });
  const int result = application.exec();
  stopCaptureFreeze(freeze);
  return result;
}
