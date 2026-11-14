#pragma once
#include <cstdint>

static inline uint32_t g_LocalAppVersion = 58;
// Shown in Settings, compared against GitHub releases, and read by scripts/package.ps1.
inline constexpr char g_AppVersionName[] = "1.0.0-beta.2";