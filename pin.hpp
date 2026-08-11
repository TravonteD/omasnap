#pragma once

#include <QString>

/** Runs a detached pinned-image layer using the current Omasnap process. */
[[nodiscard]] int runPinnedCapture(const QString &path);
