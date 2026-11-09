#pragma once

#include <QString>

namespace DesktopIntegration {
// Replace the clipboard text.
void copyText(const QString &text);
// Ask the desktop file manager to show and select the file through
// org.freedesktop.FileManager1. If no file manager answers, open the folder
// that contains it instead. Returns immediately; the request is asynchronous.
void revealFile(const QString &path);
}
