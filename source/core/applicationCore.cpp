#include "core/applicationCore.hpp"
#include "core/coreRuntime.hpp"
#include "core/triggerConfiguration.hpp"
#include <atomic>
#include <mutex>
#include <deque>
#include <functional>
#include <filesystem>
#include <fstream>
#include <cmath>
#include "controllerHotkey.hpp"
#include "dsyFileRegistry.hpp"
#include "core/startupRegistration.hpp"
#include "applicationVersion.hpp"
#include <platform_folders.h>

namespace {
bool ValidSlot(int index) { return index >= 0 && index < 4; }
ApplicationPreferences Preferences(const AppSettings& s) {
    return {s.SelectedLanguage,s.HideToTrayOnMinimize,s.HideToTrayOnStart,
        s.DisableAllBluetoothControllersOnExit,s.HideToTrayOnClose,s.StartWithWindows,s.CheckForUpdates};
}
}
struct ApplicationCore::Impl {
    CoreRuntime runtime;
    struct Command { RequestId id; std::function<std::string(CoreRuntime&)> execute; };
    std::mutex mailbox;
    std::deque<Command> pending;
    std::vector<CommandResult> results;
    std::vector<CommandResult> completed; // owner thread, released after Publish
    std::string language;
    std::shared_ptr<const std::unordered_map<std::string,std::string>> uiText;
    RequestId nextId = 1;
    size_t outstanding = 0;
    bool accepting = false;
    bool running = false; // owner thread only
    std::uint64_t revision = 0;
    std::shared_ptr<const ApplicationSnapshot> snapshot = std::make_shared<ApplicationSnapshot>();

    RequestId Enqueue(std::function<std::string(CoreRuntime&)> action) {
        std::lock_guard<std::mutex> lock(mailbox);
        if(!accepting || outstanding >= 256) return 0;
        auto id=nextId++;
        pending.push_back({id,std::move(action)});
        ++outstanding;
        return id;
    }
    void Execute() {
        std::deque<Command> batch;
        { std::lock_guard<std::mutex> lock(mailbox); batch.swap(pending); }
        for(auto& command:batch) {
            CommandResult result{command.id,CommandStatus::Succeeded,{}};
            try { result.messageKey=command.execute(runtime); }
            catch(const std::exception& e) { result.status=CommandStatus::Failed; result.messageKey=e.what(); }
            catch(...) { result.status=CommandStatus::Failed; result.messageKey="CommandFailed"; }
            completed.push_back(std::move(result));
        }
    }
    void Publish() {
        auto s=std::make_shared<ApplicationSnapshot>();
        s->appVersion=g_LocalAppVersion;
        if(language!=runtime.Settings().SelectedLanguage || !uiText) {
            language=runtime.Settings().SelectedLanguage;
            auto translated=std::make_shared<std::unordered_map<std::string,std::string>>();
            // XAML shapes Unicode itself; do not apply the legacy Arabic reversal.
            auto read=[](std::string const& code){std::ifstream f(CountryCodeToFile(code));return f?nlohmann::json::parse(f,nullptr,false):nlohmann::json{};};
            auto english=read("en"), selected=read(language);
            if(english.is_object())for(auto& item:english.items())if(item.value().is_string()) {
                auto value=item.value().get<std::string>();
                if(selected.is_object()&&selected.contains(item.key())&&selected[item.key()].is_string())value=selected[item.key()].get<std::string>();
                (*translated)[item.key()]=value;
                (*translated)["@"+item.key()+"@"]=value;
                (*translated)[item.value().get<std::string>()]=value;
            }
            uiText=std::move(translated);
        }
        s->uiText=uiText;
        s->revision=++revision;
        s->running=running;
        s->selectedController=runtime.SelectedController();
        s->preferences=Preferences(runtime.Settings());
        s->hidingAvailable=runtime.HidingAvailable();
        if(running) {
            s->emulationAvailable=runtime.Emulation().IsVigemConnected();
            s->udpActive=runtime.ModConnection().IsActive();
            s->udpPort=runtime.Settings().LocalPort;
        }
        for(int i=0;i<4;++i) {
            auto& out=s->controllers[i]; out.index=i;
            const auto& config=runtime.LegacyControllerSettings()[i];
#define OPTION(id,field,label,section,min,max,step) out.features[static_cast<size_t>(FeatureOption::id)]=config.field;
#include "core/featureOptions.inc"
#undef OPTION
            out.hotkeys={config.gyroToRightStickActivationButton,config.gyroMouseHotkey,config.mouse1Hotkey};
            for(int k=0;k<3;++k)out.hotkeyNames[k]=GetFormattedActiveButtonNames(out.hotkeys[k]);
            out.emulation=static_cast<EmulationMode>(std::clamp(config.emulatedController,0,2));
            out.profilePath=config.profilePath;
            out.audioPassthroughEnabled=config.audioPassthrough;
            out.audioFailed=runtime.AudioFailed(i);
            out.lightbarColor=config.led;
            out.lightbarBrightness=config.lightbarBrightness;
            out.baseLightbarMode=SelectedBaseLightbarMode(config.batteryBasedLightbarEnabled,config.discoMode,config.staticLightbarEnabled);
            out.gameLightbarOverrideEnabled=config.useLightbarFromEmulatedController;
            out.lightbarMode=EffectiveLightbarMode(out.baseLightbarMode,out.gameLightbarOverrideEnabled,config.gameLightbarAvailable);
            out.batteryBasedLightbarEnabled=config.batteryBasedLightbarEnabled;
            out.playerLedEnabled=!config.disablePlayerLed;
            out.leftDeadzone=config.leftStickDeadzone; out.rightDeadzone=config.rightStickDeadzone;
            out.shareAsSelect=config.ShareBtnAsSelect; out.analogWasd=config.emulateAnalogWsad;
            out.psAsWindows=config.psBtnAsWinKey; out.hidden=config.Hidden;
            for(int t=0;t<2;++t) {
                out.triggers[t].format=config.uiTriggerFormat[t]==DSX_FORMAT?"DSX":"Sony";
                out.triggers[t].mode=config.uiTriggerFormat[t]==DSX_FORMAT?config.uiSelectedDSXTriggerMode[t]:config.uiSelectedSonyTriggerMode[t];
                out.triggers[t].parameters=config.uiParameters[t];
            }
            if(!running) continue;
            s_ScePadData data{};int bus=0;s_SceControllerType type=UNKNOWN;
            out.connected=scePadReadPresentation(g_ScePad[i],&data,&bus,&type)==SCE_OK;
            if(!out.connected) continue;
            out.batteryPercent=scePadGetBatteryPercent(g_ScePad[i]);
            out.buttons=data.bitmask_buttons;out.buttonNames=GetFormattedActiveButtonNames(data.bitmask_buttons);
            out.analogTriggers={data.L2_Analog,data.R2_Analog};
            out.acceleration={data.acceleration.x,data.acceleration.y,data.acceleration.z};
            out.angularVelocity={data.angularVelocity.x,data.angularVelocity.y,data.angularVelocity.z};
            for(int t=0;t<2;++t)out.touches[t]={!bool(data.touchData.touch[t].reserve[0]),data.touchData.touch[t].x,data.touchData.touch[t].y,data.touchData.touch[t].id};
            out.hapticLevels={runtime.Audio().GetHapticLevelL(i+1),runtime.Audio().GetHapticLevelR(i+1)};
            out.capturePeak=runtime.Audio().GetCurrentCapturePeak();
            out.stickAxes={data.LeftStick.X,data.LeftStick.Y,data.RightStick.X,data.RightStick.Y};
            out.leftStickPressed=(data.bitmask_buttons&SCE_BM_L3)!=0;
            out.rightStickPressed=(data.bitmask_buttons&SCE_BM_R3)!=0;
            ++s->connectedCount;
            
            out.connection=bus==SCE_PAD_BUSTYPE_BT?ControllerConnection::Bluetooth:ControllerConnection::Usb;
            
            out.type=type==DUALSENSE?ControllerType::DualSense:type==DUALSHOCK_4?ControllerType::DualShock4:ControllerType::Unknown;
            out.name=type==DUALSENSE?"DualSense":type==DUALSHOCK_4?"DualShock 4":"Controller";
        }
        std::shared_ptr<const ApplicationSnapshot> immutable=std::move(s);
        std::atomic_store(&snapshot,std::move(immutable));
        {std::lock_guard lock(mailbox);for(auto& result:completed)results.push_back(std::move(result));completed.clear();}
    }
};
ApplicationCore::ApplicationCore():m_Impl(std::make_unique<Impl>()) {}
ApplicationCore::~ApplicationCore() { Stop(); }
ApplicationCore::StartResult ApplicationCore::Start(const std::string& path) {
    auto result=m_Impl->runtime.Start(path);
    m_Impl->running=result==CoreRuntime::StartResult::Running;
    m_Impl->Publish();
    { std::lock_guard<std::mutex> lock(m_Impl->mailbox); m_Impl->accepting=m_Impl->running; }
    return m_Impl->running?StartResult::Running:StartResult::ForwardedToExistingInstance;
}
ApplicationCore::TickResult ApplicationCore::Tick() {
    if(!m_Impl->running) return {};
    m_Impl->Execute();
    auto result=m_Impl->runtime.Tick();
    m_Impl->Publish();
    return {result.bringToFront};
}
void ApplicationCore::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_Impl->mailbox);
        m_Impl->accepting=false;
        for(auto& command:m_Impl->pending)
            m_Impl->results.push_back({command.id,CommandStatus::Cancelled,"CommandCancelled"});
        m_Impl->pending.clear();
    }
    m_Impl->runtime.Stop();
    if(m_Impl->running) { m_Impl->running=false; m_Impl->Publish(); }
}
std::shared_ptr<const ApplicationSnapshot> ApplicationCore::GetSnapshot() const {
    return std::atomic_load(&m_Impl->snapshot);
}
std::vector<ApplicationCore::CommandResult> ApplicationCore::DrainResults() {
    std::vector<CommandResult> out;
    std::lock_guard<std::mutex> lock(m_Impl->mailbox);
    out.swap(m_Impl->results);
    m_Impl->outstanding-=out.size();
    return out;
}
ApplicationCore::RequestId ApplicationCore::SelectController(int i) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i](CoreRuntime& r){r.SelectController(i);return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetEmulationMode(int i,EmulationMode mode) {
    if(!ValidSlot(i) || int(mode)<0 || int(mode)>2) return 0;
    return m_Impl->Enqueue([i,mode](CoreRuntime& r){r.LegacyControllerSettings()[i].emulatedController=int(mode);return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetControllerOption(int i,ControllerOption option,int value) {
    if(!ValidSlot(i) || int(option)<0 || int(option)>int(ControllerOption::PsAsWindows)) return 0;
    const bool deadzone=option==ControllerOption::LeftDeadzone || option==ControllerOption::RightDeadzone;
    if(value<0 || value>(deadzone?127:1)) return 0;
    return m_Impl->Enqueue([i,option,value](CoreRuntime& r){
        auto& s=r.LegacyControllerSettings()[i];
        switch(option) {
        case ControllerOption::LeftDeadzone:s.leftStickDeadzone=value;break;
        case ControllerOption::RightDeadzone:s.rightStickDeadzone=value;break;
        case ControllerOption::ShareAsSelect:s.ShareBtnAsSelect=value!=0;break;
        case ControllerOption::AnalogWasd:s.emulateAnalogWsad=value!=0;break;
        case ControllerOption::PsAsWindows:s.psBtnAsWinKey=value!=0;break;
        }
        return std::string{};
    });
}
ApplicationCore::RequestId ApplicationCore::SetControllerHidden(int i,bool hidden) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i,hidden](CoreRuntime& r){
        s_ScePadData state{};
        if(!r.HidingAvailable()) throw std::runtime_error("HidHide requires installation and administrator mode.");
        if(scePadReadState(g_ScePad[i],&state)!=SCE_OK) throw std::runtime_error("ConnectController");
        auto& s=r.LegacyControllerSettings()[i];
        s.Hidden=hidden;s.WasHidHideRanAfterLoad=false;
        return std::string{};
    });
}
ApplicationCore::RequestId ApplicationCore::SetAudioPassthroughEnabled(int i,bool enabled) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i,enabled](CoreRuntime& r){r.LegacyControllerSettings()[i].audioPassthrough=enabled;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetLightbarColor(int i,std::array<float,3> color) {
    if(!ValidSlot(i)) return 0;
    for(auto c:color) if(!std::isfinite(c)||c<0||c>1) return 0;
    return m_Impl->Enqueue([i,color](CoreRuntime& r){r.LegacyControllerSettings()[i].led=color;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetLightbarBrightness(int i,float brightness) {
    if(!ValidSlot(i)||!std::isfinite(brightness)||brightness<0||brightness>1) return 0;
    return m_Impl->Enqueue([i,brightness](CoreRuntime& r){r.LegacyControllerSettings()[i].lightbarBrightness=brightness;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetBatteryBasedLightbarEnabled(int i,bool enabled) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i,enabled](CoreRuntime& r){auto& s=r.LegacyControllerSettings()[i];s.batteryBasedLightbarEnabled=enabled;s.staticLightbarEnabled=false;if(enabled)s.discoMode=false;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetBaseLightbarMode(int i,BaseLightbarMode mode) {
    if(!ValidSlot(i)||int(mode)<0||mode>BaseLightbarMode::None)return 0;
    return m_Impl->Enqueue([i,mode](CoreRuntime& r){auto& s=r.LegacyControllerSettings()[i];
        s.batteryBasedLightbarEnabled=mode==BaseLightbarMode::Battery;s.discoMode=mode==BaseLightbarMode::ColorCycle;s.staticLightbarEnabled=mode==BaseLightbarMode::Static;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetPlayerLedEnabled(int i,bool enabled) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i,enabled](CoreRuntime& r){r.LegacyControllerSettings()[i].disablePlayerLed=!enabled;return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::LoadProfile(int i,std::string path) {
    if(!ValidSlot(i)||path.empty()) return 0;
    return m_Impl->Enqueue([i,path=std::move(path)](CoreRuntime& r){
        auto& s=r.LegacyControllerSettings()[i];
        if(!LoadSettingsFromFile(&s,path)) throw std::runtime_error("ErrorLoadConfig");
        s.WasHidHideRanAfterLoad=false; ForceControllerToNotLoadDefault(i); ApplyTriggerParameters(s);
        return std::string("ProfileLoaded");
    });
}
ApplicationCore::RequestId ApplicationCore::SaveProfile(int i,std::string path) {
    if(!ValidSlot(i)||path.empty()) return 0;
    return m_Impl->Enqueue([i,path=std::move(path)](CoreRuntime& r){
        auto& s=r.LegacyControllerSettings()[i];
        try {
            std::filesystem::path output(path); if(output.extension()!=".dsy") output+=".dsy";
            SaveSettingsToFile(s,output.string()); s.profilePath=output.string();
        } catch(...) {throw std::runtime_error("ProfileSaveFailed");}
        return std::string("ProfileSaved");
    });
}
ApplicationCore::RequestId ApplicationCore::SetDefaultProfile(int i,std::string path) {
    if(!ValidSlot(i)||path.empty()) return 0;
    return m_Impl->Enqueue([i,path=std::move(path)](CoreRuntime&){
        s_scePadSettings check{};
        if(!LoadSettingsFromFile(&check,path)) throw std::runtime_error("ErrorLoadConfig");
        std::string mac=scePadGetMacAddress(g_ScePad[i]);
        if(mac.empty()) throw std::runtime_error("ConnectController");
        mac.erase(std::remove(mac.begin(),mac.end(),':'),mac.end());
        auto dir=std::filesystem::path(sago::getDocumentsFolder())/"DualsenseZ"/"DefaultConfigs";
        try { std::filesystem::create_directories(dir);std::ofstream output(dir/mac);output<<path;output.close();
            if(!output) throw std::runtime_error("Write failed");
        } catch(...) {throw std::runtime_error("ProfileSaveFailed");}
        return std::string("DefaultProfileSet");
    });
}
ApplicationCore::RequestId ApplicationCore::RemoveDefaultProfile(int i) {
    if(!ValidSlot(i)) return 0;
    return m_Impl->Enqueue([i](CoreRuntime&){
        std::string mac=scePadGetMacAddress(g_ScePad[i]);
        if(mac.empty()) throw std::runtime_error("ConnectController");
        try {RemoveDefaultConfigByMac(mac);}catch(...){throw std::runtime_error("ProfileSaveFailed");}
        return std::string("DefaultProfileRemoved");
    });
}
ApplicationCore::RequestId ApplicationCore::SetPreferences(ApplicationPreferences p) {
    if(g_LanguageName.find(p.language)==g_LanguageName.end()) return 0;
    return m_Impl->Enqueue([p=std::move(p)](CoreRuntime& r){
        auto& s=r.Settings(); const bool languageChanged=s.SelectedLanguage!=p.language;
        s.SelectedLanguage=p.language;s.HideToTrayOnMinimize=p.hideToTrayOnMinimize;
        s.HideToTrayOnStart=p.hideToTrayOnStart;s.DisableAllBluetoothControllersOnExit=p.disconnectBluetoothOnExit;
        s.HideToTrayOnClose=p.hideToTrayOnClose;s.CheckForUpdates=p.checkForUpdates;
        SaveAppSettings(&s);
        if(languageChanged) r.Translations().ReadStringsFromJson(CountryCodeToFile(p.language));
#ifdef WINDOWS
        // The registry is touched only when the user actually flips the option.
        if(p.startWithWindows!=s.StartWithWindows) {
            if(!StartupRegistration::Set(p.startWithWindows))throw std::runtime_error("StartupRegistrationFailed");
            s.StartWithWindows=p.startWithWindows;
        }
#endif
        return std::string{};
    });
}

ApplicationCore::RequestId ApplicationCore::SetFeature(int i,FeatureOption option,double value) {
    const auto n=static_cast<size_t>(option);
    if(!ValidSlot(i)||n>=FeatureDescriptors().size()||!std::isfinite(value))return 0;
    const auto& d=FeatureDescriptors()[n];
    if(value<d.minimum||value>d.maximum||(d.step==1&&std::floor(value)!=value))return 0;
    return m_Impl->Enqueue([i,option,value](CoreRuntime& r){
        auto& s=r.LegacyControllerSettings()[i];
        switch(option) {
#define OPTION(id,field,label,section,min,max,step) case FeatureOption::id:s.field=static_cast<std::remove_reference_t<decltype(s.field)>>(value);break;
#include "core/featureOptions.inc"
#undef OPTION
        default:break;
        }
        if(option==FeatureOption::Disco && value!=0){s.batteryBasedLightbarEnabled=false;s.staticLightbarEnabled=false;}
        if(option==FeatureOption::GyroStick||option==FeatureOption::GyroArmed)
            scePadSetMicLed(g_ScePad[i],s.gyroToRightStick&&s.gyroToRightStickPermanent);
        return std::string{};
    });
}
ApplicationCore::RequestId ApplicationCore::SetHotkey(int i,HotkeyTarget target,std::uint32_t buttons) {
    if(!ValidSlot(i)||int(target)<0||int(target)>2)return 0;
    return m_Impl->Enqueue([i,target,buttons](CoreRuntime& r){auto& s=r.LegacyControllerSettings()[i];
        if(target==HotkeyTarget::GyroStick)s.gyroToRightStickActivationButton=buttons;
        else if(target==HotkeyTarget::GyroMouse)s.gyroMouseHotkey=buttons;else s.mouse1Hotkey=buttons;
        return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::SetTrigger(int i,int side,bool dsx,int mode,std::array<int,11> p) {
    const auto& modes=TriggerModes(dsx);
    if(!ValidSlot(i)||side<0||side>1||mode<0||mode>=int(modes.size()))return 0;
    const auto& schema=modes[mode];
    for(size_t n=0;n<schema.parameters.size();++n)
        if(p[n]<schema.parameters[n].minimum||p[n]>schema.parameters[n].maximum)return 0;
    if((!dsx&&(mode==2||mode==4))||(dsx&&(mode==13||mode==14)))if(p[0]>=p[1])return 0;
    if(dsx&&(mode==15||mode==17)&&p[0]>p[1])return 0;
    if(dsx&&mode==14&&p[2]>p[3])return 0;
    return m_Impl->Enqueue([i,side,dsx,mode,p](CoreRuntime& r){auto& s=r.LegacyControllerSettings()[i];
        s.uiTriggerFormat[side]=dsx?1:0;s.uiParameters[side]=p;
        (dsx?s.currentDSXItem:s.currentSonyItem)[side]=mode;
        if(side==0)s.isLeftUsingDsxTrigger=dsx;else s.isRightUsingDsxTrigger=dsx;
        ApplyTriggerParameters(s);return std::string{};});
}
ApplicationCore::RequestId ApplicationCore::AssociateProfiles() {
    return m_Impl->Enqueue([](CoreRuntime&){
#ifdef WINDOWS
        RegisterFileAssociation();
#endif
        return std::string("Profile file association registered");});
}
