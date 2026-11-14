#include "core/coreRuntime.hpp"
#include "core/triggerConfiguration.hpp"
#include "core/batteryLightbar.hpp"
#include "core/startupRegistration.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <platform_folders.h>

namespace {
std::atomic<bool> activeRuntime{false};
// Every controller setting of the last session, one entry per slot, in the profile format.
std::filesystem::path SessionFile() {return std::filesystem::path(sago::getDocumentsFolder())/"DualsenseZ"/"Session.json";}
// The same fields Stop() saves as the session's lighting preferences.
void CopyLighting(const s_scePadSettings& from,s_scePadSettings& to) {
    to.led=from.led;to.staticLightbarEnabled=from.staticLightbarEnabled;to.batteryBasedLightbarEnabled=from.batteryBasedLightbarEnabled;
    to.discoMode=from.discoMode;to.discoModeSpeed=from.discoModeSpeed;to.lightbarBrightness=from.lightbarBrightness;
    to.audioToLed=from.audioToLed;to.useLightbarFromEmulatedController=from.useLightbarFromEmulatedController;
    to.disablePlayerLed=from.disablePlayerLed;to.brightness=from.brightness;
}
}

CoreRuntime::~CoreRuntime() { Stop(); }
CoreRuntime::StartResult CoreRuntime::Start(const std::string& profilePath) {
    if(m_Started) throw std::logic_error("CoreRuntime is a single-session runtime");
    bool expected=false;
    if(!activeRuntime.compare_exchange_strong(expected,true))
        throw std::logic_error("Only one active CoreRuntime is allowed per process");
    m_OwnsSession=true;
    m_Started=true;
    try {
        ResetDefaultProfileSession();
        s_ScePadInitParam init{}; init.allowBT=true;
        scePadInit3(&init); m_Initialized=true;
        for(int i=0;i<4;++i) g_ScePad[i]=scePadOpen(i+1,0,0);
        scePadSetParticularMode(true);
        LoadAppSettings(&m_AppSettings);
#ifdef WINDOWS
        m_AppSettings.StartWithWindows=StartupRegistration::IsEnabled();
#endif
        if(m_AppSettings.LightingPreferences.is_array())for(int i=0;i<4&&i<int(m_AppSettings.LightingPreferences.size());++i) {
            const auto& saved=m_AppSettings.LightingPreferences[i];if(!saved.is_object())continue;
            try {
                auto s=m_Settings[i];
                s.led=saved.value("led",s.led);s.staticLightbarEnabled=saved.value("staticLightbarEnabled",true);
                s.batteryBasedLightbarEnabled=saved.value("batteryBasedLightbarEnabled",false);s.discoMode=saved.value("discoMode",false);
                s.lightbarBrightness=std::clamp(saved.value("lightbarBrightness",1.f),0.f,1.f);s.discoModeSpeed=saved.value("discoModeSpeed",0.1f);
                s.audioToLed=saved.value("audioToLed",false);s.useLightbarFromEmulatedController=saved.value("useLightbarFromEmulatedController",s.useLightbarFromEmulatedController);
                s.disablePlayerLed=saved.value("disablePlayerLed",s.disablePlayerLed);s.brightness=saved.value("brightness",s.brightness);
                for(auto& channel:s.led)channel=std::clamp(channel,0.f,1.f);
                m_Settings[i]=s;m_LightingRestored[i]=true;
            } catch(...) { /* Preserve defaults for an invalid saved lighting entry. */ }
        }
        SaveAppSettings(&m_AppSettings);
        // Bring back the whole last session; the lighting preferences above remain the fallback when there is none.
        try {
            std::ifstream input(SessionFile());
            nlohmann::json session;
            if(input)input>>session;
            if(session.is_array())for(int i=0;i<4&&i<int(session.size());++i) {
                const auto& saved=session[i];if(!saved.is_object())continue;
                auto s=saved.get<s_scePadSettings>();
                s.lightbarBrightness=std::clamp(saved.value("lightbarBrightness",1.f),0.f,1.f);
                s.staticLightbarEnabled=saved.value("staticLightbarEnabled",!s.batteryBasedLightbarEnabled&&!s.discoMode);
                for(auto& channel:s.led)channel=std::clamp(channel,0.f,1.f);
                s.profilePath=saved.value("profilePath",std::string{});
                s.udpConfig=false;s.gameLightbarAvailable=false;s.WasHidHideRanAfterLoad=false;
                m_Settings[i]=s;m_SessionRestored[i]=true;
            }
        } catch(...) { LOGE("Failed to load the last session's controller settings"); }
        LoadGames();
        m_Udp=std::make_unique<UDP>(m_AppSettings.LocalPort);
        if(m_Udp->IsConnectedInsteadOfBinded()) {
            if(!profilePath.empty()) m_Udp->SendConfigPathToAnotherInstance(profilePath);
            else m_Udp->BringOtherInstanceToFront();
            Stop(); return StartResult::ForwardedToExistingInstance;
        }
        m_Audio=std::make_unique<AudioPassthrough>();
        m_Vigem=std::make_unique<Vigem>(m_Workers,*m_Udp);
        m_Mapper=std::make_unique<KeyboardMouseMapper>(m_Workers);
        m_Strings.ReadStringsFromJson(CountryCodeToFile(m_AppSettings.SelectedLanguage));
        if(!profilePath.empty()) {
            ForceControllerToNotLoadDefault(0);
            LoadSettingsFromFile(&m_Settings[0],profilePath);
        }
        m_IsAdminWindows=IsRunningAsAdministratorWindows();
        m_HidingAvailable=m_IsAdminWindows && !getHidHideExecutablePath().empty();
#ifdef WINDOWS
        RegisterApplicationWithHidHide();
#endif
        m_Workers.Publish(m_Settings,m_SelectedController);
        m_Running=true;
        return StartResult::Running;
    } catch(...) { Stop(); throw; }
}
void CoreRuntime::SelectController(int index) {
    m_SelectedController=std::clamp(index,0,3);
}
CoreRuntime::TickResult CoreRuntime::Tick() {
    if(!m_Running) return {};
    m_Workers.Apply(m_Settings);
    s_ScePadData selectedState{};
    if(scePadReadState(g_ScePad[m_SelectedController],&selectedState)!=SCE_OK) {
        for(int i=0;i<4;++i) { s_ScePadData state{};
            if(scePadReadState(g_ScePad[i],&state)==SCE_OK) {m_SelectedController=i;break;}
        }
    }
    m_Vigem->SetSelectedController(m_SelectedController);
    m_Udp->SetVibrationToUdpConfig(m_Settings[m_SelectedController].rumbleFromEmulatedController);
    m_Audio->Validate();
    ScanGames();
    Synchronize();
    for(int i=0;i<4;++i) {
        // A restored last session wins over the controller's default profile, which would otherwise
        // reset it on every launch; the default profile applies only when there is no session to restore.
        // Loading a profile by hand still applies all of it.
        const auto previous=m_Settings[i];
        if(!m_SessionRestored[i]&&LoadDefaultConfig(i,&m_Settings[i])&&m_LightingRestored[i])CopyLighting(previous,m_Settings[i]);
        // Resolve legacy profiles with both automatic flags to one base mode.
        if(m_Settings[i].batteryBasedLightbarEnabled)m_Settings[i].discoMode=false;
        auto settings=i==m_SelectedController && m_Udp->IsActive()?m_Udp->GetSettings():m_Settings[i];
        s_ScePadData state{};
        const bool connected=scePadReadState(g_ScePad[i],&state)==SCE_OK;
        if(!connected || settings.emulatedController!=int(EmulatedController::DUALSHOCK4)) {
            m_Settings[i].gameLightbarAvailable=false;settings.gameLightbarAvailable=false;
        }
        const auto now=std::chrono::steady_clock::now();
        if(connected && !m_LastConnected[i]){m_ConnectedAt[i]=now;m_LightbarResends[i]=0;m_LightbarNudged[i]=false;}
        if(!connected || !m_LastConnected[i])m_LastLightbar[i]={-1,-1,-1};
        m_LastConnected[i]=connected;
        const auto band=connected && settings.batteryBasedLightbarEnabled
            ? BatteryBandForPercent(scePadGetBatteryPercent(g_ScePad[i])):BatteryLightbarBand::Unavailable;
        const auto mode=EffectiveLightbarMode(SelectedBaseLightbarMode(settings.batteryBasedLightbarEnabled,settings.discoMode,settings.staticLightbarEnabled),settings.useLightbarFromEmulatedController,settings.gameLightbarAvailable);
        applySettings(i,settings,*m_Audio,mode,int(band),m_LastLightbar[i]);
        // A color sent right after the controller opens can be lost: the device is still running its
        // start-up light fade, and the HID layer only writes a color that differs from the last one.
        // So the color is sent again a few times after connecting, via a one-step nudge the eye cannot see.
        static constexpr int resendAfterMs[]{800,2000,4000};
        if(connected && m_LightbarResends[i]<3) {
            if(m_LightbarNudged[i]) {m_LastLightbar[i]={-1,-1,-1};m_LightbarNudged[i]=false;++m_LightbarResends[i];}
            else if(now-m_ConnectedAt[i]>=std::chrono::milliseconds(resendAfterMs[m_LightbarResends[i]])) {
                if(m_LastLightbar[i][0]>=0) {
                    s_SceLightBar nudge{uint8_t(m_LastLightbar[i][0]^1),uint8_t(m_LastLightbar[i][1]),uint8_t(m_LastLightbar[i][2])};
                    m_LightbarNudged[i]=scePadSetLightBar(g_ScePad[i],&nudge)==SCE_OK;
                }
                if(!m_LightbarNudged[i])++m_LightbarResends[i];
            }
        }
    }
    if(m_Udp->SettingsFromOtherInstanceAvailable())
        m_Settings[m_SelectedController]=m_Udp->GetSettingsFromOtherInstance();
    m_Workers.Publish(m_Settings,m_SelectedController);
    return {m_Udp->AwaitingBringToFront()};
}
void CoreRuntime::Stop() {
    const bool wasRunning=m_Running;
    m_Running=false;
    // Stop all users of shared settings/handles before releasing either.
    m_Mapper.reset();
    m_Vigem.reset(); // UDP remains alive until ViGEm joins.
    m_Audio.reset();
    m_Udp.reset();
    // Hand the controller back to the user's own settings first, so the session saved below is theirs.
    if(wasRunning)DeactivateGame();
    if(wasRunning)try {
        m_AppSettings.LightingPreferences=nlohmann::json::array();
        for(const auto& s:m_Settings)m_AppSettings.LightingPreferences.push_back({
            {"led",s.led},{"staticLightbarEnabled",s.staticLightbarEnabled},{"batteryBasedLightbarEnabled",s.batteryBasedLightbarEnabled},
            {"discoMode",s.discoMode},{"discoModeSpeed",s.discoModeSpeed},{"lightbarBrightness",s.lightbarBrightness},
            {"audioToLed",s.audioToLed},{"useLightbarFromEmulatedController",s.useLightbarFromEmulatedController},
            {"disablePlayerLed",s.disablePlayerLed},{"brightness",s.brightness}});
        SaveAppSettings(&m_AppSettings);
        auto session=nlohmann::json::array();
        for(const auto& s:m_Settings) {
            nlohmann::json entry=s;
            entry["lightbarBrightness"]=s.lightbarBrightness;entry["staticLightbarEnabled"]=s.staticLightbarEnabled;entry["profilePath"]=s.profilePath;
            session.push_back(std::move(entry));
        }
        std::filesystem::create_directories(SessionFile().parent_path());
        std::ofstream(SessionFile())<<session.dump(4);
    } catch(...) { LOGE("Failed to save the session's controller settings"); }
    if(m_Initialized) {
#ifdef WINDOWS
        if(wasRunning) {
            if(m_IsAdminWindows) for(int i=0;i<4;++i) UnhideController(scePadGetPath(g_ScePad[i]));
            if(m_AppSettings.DisableAllBluetoothControllersOnExit)
                for(int i=0;i<4;++i) DisableBluetoothDevice(scePadGetMacAddress(g_ScePad[i]));
        }
#endif
        scePadTerminate();
        for(auto& handle:g_ScePad) handle=0;
        m_Initialized=false;
    }
    if(m_OwnsSession) {m_OwnsSession=false;activeRuntime.store(false);}
}
void CoreRuntime::Synchronize() {
    for (int i = 0; i < 4; ++i) {
        auto& s = m_Settings[i];
        ApplyTriggerParameters(s);
        s_ScePadData state{};
        const bool connected = scePadReadState(g_ScePad[i], &state) == SCE_OK;
        if (m_Vigem->IsVigemConnected())
            m_Vigem->PlugControllerByIndex(i, connected ? std::clamp(s.emulatedController,0,2) : 0);
        const bool wantAudio = connected && s.audioPassthrough;
        if (wantAudio && !m_AudioStarted[i]) {
            m_AudioStarted[i] = m_Audio->StartByUserId(i+1);
            m_AudioFailed[i] = !m_AudioStarted[i];
            if (m_AudioFailed[i]) s.audioPassthrough = false;
        } else if (!wantAudio && m_AudioStarted[i]) {
            m_Audio->StopByUserId(i+1);
            m_AudioStarted[i] = false;
        }
#ifdef WINDOWS
        if (connected && !s.WasHidHideRanAfterLoad && m_IsAdminWindows && !getHidHideExecutablePath().empty()) {
            const auto path = scePadGetPath(g_ScePad[i]);
            if (s.Hidden) HideController(path); else UnhideController(path);
            s.WasHidHideRanAfterLoad = true;
        }
#endif
    }
}

