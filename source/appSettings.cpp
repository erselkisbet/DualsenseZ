#include "appSettings.hpp"
#include <platform_folders.h>
#include <filesystem>
#include <fstream>
#include "log.hpp"
#ifdef WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Only languages the app ships are offered; anything else starts in English.
static std::string SystemLanguage() {
#ifdef WINDOWS
	return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_TURKISH ? "tr" : "en";
#else
	return "en";
#endif
}

static inline std::filesystem::path directory = std::filesystem::path(sago::getDocumentsFolder() + "/DualsenseZ/");
static inline std::filesystem::path filePath = directory / "Config.json";

void SaveAppSettings(AppSettings* appSettings) {
	nlohmann::json j = *appSettings;

	if (!std::filesystem::is_directory(directory))
		std::filesystem::create_directories(directory);

	std::ofstream(filePath) << j.dump(4);
}

void LoadAppSettings(AppSettings* appSettings) {
	try {
		if (std::filesystem::exists(filePath)) {
			std::ifstream ifs(filePath);
			if (!ifs) return;
			nlohmann::json j;
			ifs >> j;
			*appSettings = j.get<AppSettings>();
		}
		else {
			appSettings->SelectedLanguage = SystemLanguage();
		}
	}
	catch (...) {
		LOGE("Failed to load application config file");
	}
	if (g_LanguageName.find(appSettings->SelectedLanguage) == g_LanguageName.end())
		appSettings->SelectedLanguage = "en";
}
