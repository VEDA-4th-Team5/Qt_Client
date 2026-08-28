#ifndef UIFONTSCALE_H
#define UIFONTSCALE_H

#include <QString>

class QApplication;

namespace UiFontScale {

inline constexpr int CompactPercent = 90;
inline constexpr int DefaultPercent = 100;
inline constexpr int LargePercent = 110;

int normalizePercent(int percent);
int loadPercent(const QString &configPath);
void initialize(QApplication &app, const QString &configPath);
bool setPercent(int percent, QString *errorMessage = nullptr);
int currentPercent();

} // namespace UiFontScale

#endif
