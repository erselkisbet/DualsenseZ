#pragma once
#include "applicationSnapshot.hpp"
#include <memory>
#include <vector>
// Start/Tick/Stop: one serialized runtime owner thread. Snapshot, results and
// commands: callable from UI threads. Join all callers before destruction.
class ApplicationCore {
public:
    enum class StartResult { Running, ForwardedToExistingInstance };
    struct TickResult { bool bringToFront = false; };
    using RequestId = std::uint64_t;
    enum class CommandStatus { Succeeded, Failed, Cancelled };
    struct CommandResult { RequestId id; CommandStatus status; std::string messageKey; };
    ApplicationCore();
    ~ApplicationCore();
    ApplicationCore(const ApplicationCore&) = delete;
    ApplicationCore& operator=(const ApplicationCore&) = delete;
    StartResult Start(const std::string& profilePath = "");
    TickResult Tick();
    void Stop();
    std::shared_ptr<const ApplicationSnapshot> GetSnapshot() const;
    std::vector<CommandResult> DrainResults();
    // 0 = invalid input, not running, or 256 outstanding requests. Drain results
    // regularly. Each accepted request completes exactly once, including Stop.
    RequestId SetFeature(int index,FeatureOption option,double value);
    RequestId SetHotkey(int index,HotkeyTarget target,std::uint32_t buttons);
    RequestId SetTrigger(int index,int side,bool dsx,int mode,std::array<int,11> parameters);
    RequestId AssociateProfiles();
    RequestId SelectController(int index);
    RequestId SetControllerOption(int index, ControllerOption option, int value);
    RequestId SetControllerHidden(int index, bool hidden);
    RequestId SetEmulationMode(int index, EmulationMode mode);
    RequestId SetAudioPassthroughEnabled(int index, bool enabled);
    RequestId SetLightbarColor(int index, std::array<float,3> color);
    RequestId SetLightbarBrightness(int index, float brightness);
    RequestId SetBaseLightbarMode(int index, BaseLightbarMode mode);
    RequestId SetBatteryBasedLightbarEnabled(int index, bool enabled);
    RequestId SetPlayerLedEnabled(int index, bool enabled);
    RequestId LoadProfile(int index, std::string path);
    RequestId SaveProfile(int index, std::string path);
    RequestId SetDefaultProfile(int index, std::string path);
    RequestId RemoveDefaultProfile(int index);
    RequestId SetPreferences(ApplicationPreferences preferences);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
