#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <nlohmann/json.hpp>
#include <unordered_map>

static std::unordered_map<std::string, std::string> g_LanguageName = {
	{"en", "English"},
	{"tr", "Türkçe"},
};

struct AppSettings {
	bool DisableAllBluetoothControllersOnExit = false;
	std::string SelectedLanguage = "en"; // First run follows the Windows display language; see LoadAppSettings.
	bool HideToTrayOnMinimize = false;
	bool HideToTrayOnStart = false;
	bool HideToTrayOnClose = false;
	bool StartWithWindows = false; // Mirrors the registry Run entry; deliberately not saved to the config file.
	uint16_t LocalPort = 6969;
	nlohmann::json LightingPreferences = nlohmann::json::array();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	AppSettings,
	DisableAllBluetoothControllersOnExit,
	SelectedLanguage,
	HideToTrayOnMinimize,
	HideToTrayOnStart,
	HideToTrayOnClose,
	LocalPort,
	LightingPreferences
);

void SaveAppSettings(AppSettings* appSettings);
void LoadAppSettings(AppSettings* appSettings);

#endif
