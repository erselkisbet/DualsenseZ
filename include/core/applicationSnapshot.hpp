#pragma once
#include <array>
#include "featureModel.hpp"
#include "lightingLevels.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
enum class EmulationMode { None, Xbox360, DualShock4 };
enum class ControllerType { Unknown, DualSense, DualShock4 };
enum class ControllerConnection { Disconnected, Usb, Bluetooth };
enum class ControllerOption { LeftDeadzone, RightDeadzone, ShareAsSelect, AnalogWasd, PsAsWindows };
struct ApplicationPreferences {
    std::string language = "en";
    bool hideToTrayOnMinimize = false;
    bool hideToTrayOnStart = false;
    bool disconnectBluetoothOnExit = false;
    bool hideToTrayOnClose = false;
    bool startWithWindows = false;
    bool checkForUpdates = true;
};
struct TriggerSnapshot {
    std::string format;
    std::string mode;
    std::array<int,11> parameters{};
};
struct ControllerSnapshot {
    std::array<double,static_cast<size_t>(FeatureOption::Count)> features{};
    std::array<std::uint32_t,3> hotkeys{};
    std::array<std::string,3> hotkeyNames{};
    std::array<TouchSnapshot,2> touches{};
    std::array<float,3> acceleration{},angularVelocity{};
    std::array<int,2> analogTriggers{};
    std::array<float,2> hapticLevels{};
    float capturePeak=0;
    std::uint32_t buttons=0;
    std::string buttonNames;
    int index = 0;
    bool connected = false;
    ControllerType type = ControllerType::Unknown;
    std::string name;
    ControllerConnection connection = ControllerConnection::Disconnected;
    EmulationMode emulation = EmulationMode::None;
    std::string profilePath;
    bool audioPassthroughEnabled = false;
    bool audioFailed = false;
    std::array<float,3> lightbarColor{};
    float lightbarBrightness = 1.0f;
    LightbarMode lightbarMode = LightbarMode::Static;
    BaseLightbarMode baseLightbarMode = BaseLightbarMode::Static;
    bool gameLightbarOverrideEnabled = false;
    bool batteryBasedLightbarEnabled = false;
    int batteryPercent = -1;
    bool playerLedEnabled = true;
    int leftDeadzone = 0, rightDeadzone = 0;
    bool shareAsSelect = false, analogWasd = false, psAsWindows = false;
    bool hidden = false;
    std::array<int,4> stickAxes{128,128,128,128};
    bool leftStickPressed = false, rightStickPressed = false;
    std::array<TriggerSnapshot,2> triggers;
};
struct ApplicationSnapshot {
    std::uint32_t appVersion=0;
    std::shared_ptr<const std::unordered_map<std::string,std::string>> uiText;
    std::uint64_t revision = 0;
    bool running = false;
    int selectedController = 0;
    int connectedCount = 0;
    bool emulationAvailable = false;
    bool udpActive = false;
    int udpPort = 0;
    bool hidingAvailable = false;
    ApplicationPreferences preferences;
    std::array<ControllerSnapshot,4> controllers;
};
