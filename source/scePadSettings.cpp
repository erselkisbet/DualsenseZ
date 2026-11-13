#include "scePadSettings.hpp"
#include "led.hpp"
#include "core/batteryLightbar.hpp"
#include <algorithm>
#include <fstream>

#include <platform_folders.h>

static std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
// <Mac address, was loaded>
static std::unordered_map<std::string, bool> loadList;
void ResetDefaultProfileSession() {loadList.clear();}

void SaveSettingsToFile(const s_scePadSettings &s, const std::string &filepath)
{
    const std::filesystem::path filePath(filepath);
    if (!filePath.parent_path().empty()) std::filesystem::create_directories(filePath.parent_path());
    nlohmann::json j = s;
    j["lightbarBrightness"] = s.lightbarBrightness;
    j["staticLightbarEnabled"] = s.staticLightbarEnabled;
    std::ofstream output;
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.open(filePath);
    output << j.dump(4);
    output.close();
}

bool LoadSettingsFromFile(s_scePadSettings *s, const std::string &filepath)
{
	try
	{
		std::ifstream ifs(filepath);
		if (!ifs)
			return false;
		nlohmann::json j;
		ifs >> j;
		*s = j.get<s_scePadSettings>();
		s->lightbarBrightness = j.value("lightbarBrightness", 1.0f);
		s->staticLightbarEnabled = j.value("staticLightbarEnabled", !s->batteryBasedLightbarEnabled&&!s->discoMode);
		s->profilePath = filepath;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool GetDefaultConfigFromMac(const std::string &mac, s_scePadSettings *s)
{
	std::string cleanMac = mac;
	cleanMac.erase(std::remove(cleanMac.begin(), cleanMac.end(), ':'), cleanMac.end());
	std::filesystem::path filePath = std::filesystem::path(sago::getDocumentsFolder() + "/DualsenseZ/DefaultConfigs/" + cleanMac);

	if (std::filesystem::exists(filePath))
	{
		std::ifstream file(filePath);
		std::string configPath = "";
		std::getline(file, configPath);
		file.close();
		return LoadSettingsFromFile(s, configPath);
	}

	return false;
}

bool RemoveDefaultConfigByMac(const std::string &mac)
{
	std::string cleanMac = mac;
	cleanMac.erase(std::remove(cleanMac.begin(), cleanMac.end(), ':'), cleanMac.end());
	std::filesystem::path filePath = std::filesystem::path(sago::getDocumentsFolder() + "/DualsenseZ/DefaultConfigs/" + cleanMac);

	if (std::filesystem::exists(filePath))
	{
		std::filesystem::remove(filePath);
		return true;
	}

	return false;
}

bool LoadDefaultConfig(int currentController, s_scePadSettings *s)
{
	std::string macAddress = scePadGetMacAddress(g_ScePad[currentController]);

	if (macAddress != "" && !loadList[macAddress])
	{
		if (GetDefaultConfigFromMac(macAddress, s))
		{
			s->WasHidHideRanAfterLoad = false;
			loadList[macAddress] = true;
			return true;
		}
	}
	return false;
}

void ForceControllerToNotLoadDefault(int controller)
{
	std::string macAddress = scePadGetMacAddress(g_ScePad[controller]);
	loadList[macAddress] = true;
}

std::string ScePadSettingsToString(s_scePadSettings *s)
{
	nlohmann::json j = *s;
	j["lightbarBrightness"] = s->lightbarBrightness;
	j["staticLightbarEnabled"] = s->staticLightbarEnabled;
	return j.dump();
}

bool LoadSettingsFromString(s_scePadSettings *s, const std::string &String)
{
	try
	{
		nlohmann::json j = nlohmann::json::parse(String);
		*s = j.get<s_scePadSettings>();
		s->lightbarBrightness = j.value("lightbarBrightness", 1.0f);
		s->staticLightbarEnabled = j.value("staticLightbarEnabled", !s->batteryBasedLightbarEnabled&&!s->discoMode);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool SaveSettingsFromString(const std::string &String, const std::string &Path)
{
	try
	{
		s_scePadSettings settings = {};
		nlohmann::json j = nlohmann::json::parse(String);
		settings = j.get<s_scePadSettings>();
		settings.lightbarBrightness = j.value("lightbarBrightness", 1.0f);
		settings.staticLightbarEnabled = j.value("staticLightbarEnabled", !settings.batteryBasedLightbarEnabled&&!settings.discoMode);
		SaveSettingsToFile(settings, Path);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void applySettings(uint32_t index, s_scePadSettings settings, AudioPassthrough &audio, LightbarMode mode, int batteryBand, std::array<int,3>& lastLightbar)
{
	auto now = std::chrono::steady_clock::now();
	auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
	float elapsed = std::chrono::duration<float>(now - startTime).count();

	float discoModeTime = fmod(elapsed * settings.discoModeSpeed, 1.0f);
	s_SceLightBar rainbowLightbar = {};
	GetRainbowColor(discoModeTime, rainbowLightbar);

	float audioPeak = audio.GetCurrentCapturePeak();
	uint8_t audioPeakUint8 = (uint8_t)scaleFloatToInt(audioPeak, 1.0);

	// One effective-color decision keeps the visible priority and device output aligned.
	s_SceLightBar lightbar{};
	bool hasColor=true;
	if (mode==LightbarMode::None) {
		hasColor=false;
		if(lastLightbar!=std::array<int,3>{-2,-2,-2} && scePadReleaseLightBar(g_ScePad[index])==SCE_OK)lastLightbar={-2,-2,-2};
	} else if (mode==LightbarMode::Battery) {
		if (batteryBand < 0) hasColor=false;
		else {
			auto color=BatteryBandColor(static_cast<BatteryLightbarBand>(batteryBand));
			lightbar={uint8_t(color[0]*255),uint8_t(color[1]*255),uint8_t(color[2]*255)};
		}
	} else if (mode==LightbarMode::ColorCycle) {
		lightbar=settings.audioToLed
			? s_SceLightBar{std::min(audioPeakUint8,rainbowLightbar.r),std::min(audioPeakUint8,rainbowLightbar.g),std::min(audioPeakUint8,rainbowLightbar.b)}
			: rainbowLightbar;
	} else if (mode==LightbarMode::Game) {
		lightbar=settings.lightbarFromEmulatedController;
	} else if (settings.audioToLed) {
		lightbar={audioPeakUint8,audioPeakUint8,audioPeakUint8};
	} else {
		lightbar={(uint8_t)scaleFloatToInt(settings.led[0],1.0f),(uint8_t)scaleFloatToInt(settings.led[1],1.0f),(uint8_t)scaleFloatToInt(settings.led[2],1.0f)};
	}
	if(hasColor) {
		const float brightness=std::clamp(settings.lightbarBrightness,0.0f,1.0f);
		lightbar.r=uint8_t(lightbar.r*brightness);
		lightbar.g=uint8_t(lightbar.g*brightness);
		lightbar.b=uint8_t(lightbar.b*brightness);
		std::array<int,3> effective{lightbar.r,lightbar.g,lightbar.b};
		if(effective!=lastLightbar && scePadSetLightBar(g_ScePad[index],&lightbar)==SCE_OK) {
			lastLightbar=effective;
		}
	}

	scePadSetPlayerLedBrightness(g_ScePad[index], settings.brightness);
	scePadSetPlayerLed(g_ScePad[index], settings.disablePlayerLed ? false : true);
	bool micLedState = settings.gyroToRightStick && settings.gyroToRightStickPermanent;
	scePadSetMicLed(g_ScePad[index], micLedState);
	if (!settings.udpConfig)
	{
		if (settings.speakerVolume == 0)
			scePadSetAudioOutPath(g_ScePad[index], SCE_PAD_AUDIO_PATH_STEREO_HEADSET);
		else
			scePadSetAudioOutPath(g_ScePad[index], settings.audioPath);
	}

	s_SceControllerType controllerType = {};
	scePadGetControllerType(g_ScePad[index], &controllerType);

	s_ScePadVolumeGain volume = {};
	volume.speakerVolume = settings.speakerVolume * 9;
	volume.micGain = settings.micGain * 6;
	scePadSetVolumeGain(g_ScePad[index], &volume);

	audio.SetHapticIntensityByUserId(index + 1, settings.hapticIntensity);
	audio.SetSpeakerVolumeByUserId(index + 1, (float)settings.speakerVolume / 8.0f);

	int l2Value = settings.rumbleToAt_swapTriggers ? settings.rumbleFromEmulatedController.smallMotor : settings.rumbleFromEmulatedController.largeMotor;
	int r2Value = settings.rumbleToAt_swapTriggers ? settings.rumbleFromEmulatedController.largeMotor : settings.rumbleFromEmulatedController.smallMotor;

	if (settings.triggersAsButtons && settings.emulatedController != (int)EmulatedController::NONE)
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		CustomTriggerHardestB(leftTrigger, settings.triggersAsButtonStartPos);
		CustomTriggerHardestB(rightTrigger, settings.triggersAsButtonStartPos);
		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else if (settings.hapticsToAt && settings.audioPassthrough)
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		float rawL = audio.GetHapticLevelL(index + 1);
		float rawR = audio.GetHapticLevelR(index + 1);

		float levelL = settings.hapticsToAt_swapTriggers ? rawR : rawL;
		float levelR = settings.hapticsToAt_swapTriggers ? rawL : rawR;

		levelL *= settings.hapticsToAt_intensity;
		levelR *= settings.hapticsToAt_intensity;

		const float noiseGate = 0.03f;
		if (levelL < noiseGate) levelL = 0.0f;
		if (levelR < noiseGate) levelR = 0.0f;

		levelL = std::clamp(levelL, 0.0f, 1.0f);
		levelR = std::clamp(levelR, 0.0f, 1.0f);

		auto applyHapticEffect = [](float level, int mode, uint8_t trigger[11])
		{
			if (level <= 0.0f)
			{
				CustomTriggerOFF(trigger);
				return;
			}

			if (mode == 0)
			{
				// Mode 0: Vibration (Pulse_B)
				uint8_t freq = static_cast<uint8_t>(std::clamp(static_cast<int>(5.0f + level * 30.0f), 5, 40));
				uint8_t intensity = static_cast<uint8_t>(std::clamp(static_cast<int>(level * 255.0f), 10, 255));
				uint8_t pos = 0;
				std::vector<uint8_t> param = { freq, intensity, pos };
				CustomTriggerBetterVibration(param, trigger);
			}
			else
			{
				// Mode 1: Dynamic Resistance (Feedback)
				uint8_t force = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 8.0f)), 1, 8));
				uint8_t start = 0;
				std::vector<uint8_t> param = { start, force };
				CustomTriggerResistance(param, trigger);
			}
		};

		applyHapticEffect(levelL, settings.hapticsToAt_mode, leftTrigger);
		applyHapticEffect(levelR, settings.hapticsToAt_mode, rightTrigger);

		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else if (settings.rumbleToAT && settings.emulatedController != (int)EmulatedController::NONE)
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		std::vector<uint8_t> l2Param;
		l2Param.push_back(std::min(l2Value, settings.rumbleToAt_frequency[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]));
		l2Param.push_back(std::min(l2Value, settings.rumbleToAt_intensity[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]));
		l2Param.push_back(settings.rumbleToAt_position[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]);

		std::vector<uint8_t> r2Param;
		r2Param.push_back(std::min(r2Value, settings.rumbleToAt_frequency[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]));
		r2Param.push_back(std::min(r2Value, settings.rumbleToAt_intensity[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]));
		r2Param.push_back(settings.rumbleToAt_position[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]);

		CustomTriggerBetterVibration(l2Param, leftTrigger);
		CustomTriggerBetterVibration(r2Param, rightTrigger);

		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else
	{
		settings.stockTriggerParam.triggerMask |= settings.isLeftUsingDsxTrigger ? 0 : SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2;
		settings.stockTriggerParam.triggerMask |= settings.isRightUsingDsxTrigger ? 0 : SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2;
		scePadSetTriggerEffect(g_ScePad[index], &settings.stockTriggerParam);

		uint8_t triggerBitmask = 0;
		triggerBitmask |= settings.isLeftUsingDsxTrigger ? SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 : 0;
		triggerBitmask |= settings.isRightUsingDsxTrigger ? SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2 : 0;
		scePadSetTriggerEffectCustom(g_ScePad[index], settings.leftCustomTrigger.data(), settings.rightCustomTrigger.data(), triggerBitmask);
	}

	if (settings.useRumbleFromEmulatedController || settings.udpConfig)
	{
		if (settings.rumbleFromEmulatedController.largeMotor > 0 || settings.rumbleFromEmulatedController.smallMotor > 0)
		{
			scePadSetVibrationMode(g_ScePad[index], SCE_PAD_RUMBLE_MODE);
		}
		else
		{
			scePadSetVibrationMode(g_ScePad[index], SCE_PAD_HAPTICS_MODE);
		}

		scePadSetVibration(g_ScePad[index], &settings.rumbleFromEmulatedController);
	}
}
