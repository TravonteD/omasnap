# Omarchy Capture Editor 1.0

A native Wayland screenshot and annotation overlay designed for Omarchy and Hyprland.
It captures the focused monitor before mapping an exclusive layer-shell surface, so the
editor never appears in its own screenshot. The editor retains annotations as movable,
resizable vector layers and preserves the monitor's native pixels on scaled displays.

## Features

- Freeform region, window, and full-monitor capture modes.
- Clean window-surface capture through Wayland image-copy protocols, with a screen-crop
  fallback when the compositor does not expose the requested surface.
- Select/move/resize layers, mouse-wheel scaling, and eight external recropping handles.
- Arrows, straight lines, smoothed freehand strokes, rectangles, numbered markers, and
  editable Neucha text.
- Per-layer preset or custom colors, undo, OCR-region capture, mesh-gradient backdrops,
  and rendered drop shadows.
- PNG clipboard output through `wl-copy` and timestamped files under
  `~/Pictures/Screenshots` by default.
- Correct native-pixel export on fractional or integer-scaled monitors.

## Platform scope

The supported target is **Wayland + Hyprland**, with Omarchy as the primary integration.
The renderer, layer surface, clipboard, and clean-window capture use Wayland protocols,
but monitor/window discovery currently calls `hyprctl` and the frozen-frame handoff uses
`hyprpicker`. Another Wayland compositor could support the application after supplying
an equivalent discovery/freeze backend; generic Wayland support is not claimed by 1.0.

Runtime commands used by the application:

- `hyprctl` and `hyprpicker`
- `grim`
- `wl-copy`
- `tesseract`
- `omarchy-notification-send` when available; notification failure does not invalidate a
  completed capture

## Install on Omarchy

Clone the repository and run the Omarchy installer:

```bash
git clone <repository-url> omarchy-capture-editor
cd omarchy-capture-editor
./install-omarchy
```

The installer uses Omarchy's package helper for missing dependencies, builds in
`~/.cache/omarchy-capture-editor`, installs under `~/.local`, and runs
`omarchy-capture-editor-setup`. The setup helper creates its own
`~/.config/hypr/omarchy_capture_editor.lua` module and appends one guarded `require(...)`
line to `hyprland.lua` after making a timestamped backup. A pre-existing
`personal_bindings.lua` is not required.

`omarchy plugin add` is intentionally not used. Omarchy plugins are Quickshell QML
extensions; they do not install native executables or system packages. A dummy shell plugin
would add a second installation mechanism without helping the screenshot workflow.

To remove only the Hyprland integration:

```bash
omarchy-capture-editor-setup --remove
```

Set `OMARCHY_CAPTURE_PREFIX` before running `install-omarchy` to use a prefix other than
`~/.local`.

### Manual Arch Linux build

Install the complete build/runtime dependency set:

```bash
sudo pacman -S --needed \
  base-devel cmake ninja pkgconf qt6-base layer-shell-qt \
  wayland wayland-protocols hyprland grim hyprpicker wl-clipboard \
  tesseract tesseract-data-eng
```

Build and install:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build
omarchy-capture-editor-setup  # Omarchy integration; omit on other desktops
```

The install step places:

- `~/.local/bin/omarchy-capture-editor`
- `~/.local/bin/omarchy-capture-editor-setup`
- `~/.local/share/applications/omarchy-capture-editor.desktop`
- `~/.local/share/omarchy-capture-editor/omarchy_capture_editor.lua`
- `~/.local/share/licenses/omarchy-capture-editor/Neucha-OFL.txt`

Ensure `~/.local/bin` is on `PATH`, then verify the installed CLI:

```bash
omarchy-capture-editor --version
omarchy-capture-editor --help
```

## CLI capture modes

Running without arguments opens freeform region selection:

```bash
omarchy-capture-editor
```

Explicit starting modes:

```bash
omarchy-capture-editor --capture-region
omarchy-capture-editor --capture-window
omarchy-capture-editor --capture-fullscreen
```

Compatibility positional names are also accepted:

```bash
omarchy-capture-editor region
omarchy-capture-editor windows
omarchy-capture-editor fullscreen
omarchy-capture-editor smart       # maps to region selection
```

These options choose what is initially selected; the editor still controls whether the
result is copied, saved, or both.

Environment overrides:

```bash
OMARCHY_SCREENSHOT_DIR="$HOME/Pictures/Captures" omarchy-capture-editor
OMARCHY_OCR_LANGS="eng+deu" omarchy-capture-editor
```

Install the corresponding Tesseract language data before adding a language to
`OMARCHY_OCR_LANGS`.

## Standard Omarchy binding

Standard Omarchy binds unmodified `PRINT` to `omarchy-capture-screenshot`. Its modified
Print chords remain separate actions:

- `ALT + PRINT`: screen recording
- `SUPER + PRINT`: color picker
- `SUPER + CTRL + PRINT`: one-shot OCR

`omarchy-capture-editor-setup` overrides only the exact standard screenshot chord. It
installs this dedicated module:

```lua
-- ~/.config/hypr/omarchy_capture_editor.lua
hl.unbind("PRINT")
o.bind("PRINT", "Screenshot", "omarchy-capture-editor")

hl.layer_rule({
  match = { namespace = "^omarchy-capture-editor$" },
  no_anim = true,
  animation = "none",
})
```

The helper appends `require("hypr.omarchy_capture_editor")` to `hyprland.lua`, after
Omarchy's defaults have already loaded. Re-running it is idempotent. It neither creates nor
depends on `personal_bindings.lua`, and it does not claim `SUPER + SHIFT + S` or any other
nonstandard screenshot chord.

Verify the live result:

```bash
hyprctl configerrors
hyprctl binds -j | jq -c \
  '[.[] | select(.description == "Screenshot") | {modmask,key,description}]'
```

`hyprctl configerrors` should print nothing; the binding query should return exactly one
entry with `modmask: 0` and `key: "PRINT"`.

## Controls

### Capture selection

| Input | Action |
|---|---|
| Drag | Select a region |
| `Space` | Toggle region/window selection |
| `SUPER + Arrow` | Move among windows in window mode |
| `Enter` | Capture the highlighted window |
| `Ctrl+A` | Select the full focused monitor |
| `Esc`, `Esc` | Dismiss |

### Annotation editor

| Input | Action |
|---|---|
| `V` | Select/move/resize layers; wheel scales the selected layer |
| `A` | Arrow |
| `L` | Straight line |
| `F` | Freehand stroke |
| `C` | Numbered marker |
| `R` | Rectangle |
| `T` | Neucha text |
| `O` | Drag an OCR region and copy recognized text |
| `B` | Cycle backdrop |
| `1`–`6` | Set annotation color |
| Wheel | Scale selected layer or change active tool size |
| Double-click text | Reopen text editing |
| `Delete` | Delete selected layer |
| `Ctrl+Z` | Undo |
| `Ctrl+C` | Copy PNG only |
| `Ctrl+S` | Save PNG only |
| `Enter` | Copy and save |
| `Esc` | Switch to Arrow; press again to close |

Creation tools return to Select after one placement. In Select mode, the eight blue/white
handles outside the image recrop its corners or edges.

## Development and verification

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen \
  ./build/omarchy-capture-editor-smoke /tmp/omarchy-capture-editor-smoke
```

The smoke executable exercises region/window/fullscreen startup modes, capture selection,
annotation tools, vector movement and scaling, text editing, OCR, native-DPI output, and
external crop handles.

`.github/workflows/build-linux.yml` performs the same release build and interaction smoke
in an Arch Linux container, stages the CMake installation, and verifies that the Omarchy
setup is idempotent and removable. It uploads a versioned Linux artifact; a `v*` tag also
attaches that artifact to the corresponding GitHub release.

## Acknowledgements

The capture and annotation workflow is inspired by three excellent screenshot tools:

- [Shottr](https://shottr.cc/) — fast region/window capture, OCR, and polished backdrops.
- [Satty](https://github.com/Satty-org/Satty) — a focused, Wayland-native annotation workflow.
- [Flameshot](https://github.com/flameshot-org/flameshot) — selection-first capture and an
  approachable annotation toolbar.

Thanks to their authors and contributors for establishing the interaction patterns that made
this project possible. Omarchy Capture Editor is an independent implementation and is not
affiliated with those projects.

## Project history

This standalone repository was extracted with `git filter-repo` from the original Omarchy
system-customization repository. The former `capture-editor/` directory was promoted to
the repository root while retaining its relevant commit history.

The bundled Neucha font is distributed under the SIL Open Font License; its license is in
`assets/OFL.txt` and is installed with the application.
