// Headless consumer: deliberately includes only the framework-free public API.
#include "core/applicationCore.hpp"
#include "core/uiText.hpp"
#include "core/batteryLightbar.hpp"
#include "core/lightingLevels.hpp"
#include <atomic>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <limits>
#include <cmath>
#include <fstream>
#include <sstream>
#include <optional>
#include <platform_folders.h>

void Check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
void TestWorkerExchange();
// The core saves into the user's real settings file; put it back however the run ends.
struct SettingsFileGuard {
    std::filesystem::path path;
    std::optional<std::string> content;
    explicit SettingsFileGuard(const char* name):path(std::filesystem::path(sago::getDocumentsFolder())/"DualsenseZ"/name) {std::ifstream in(path,std::ios::binary);if(in){std::ostringstream all;all<<in.rdbuf();content=all.str();}}
    ~SettingsFileGuard() {
        std::error_code ignored;
        if(content){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<*content;}
        else std::filesystem::remove(path,ignored);
    }
};
int main(int argc,char** argv) {
    SettingsFileGuard settingsFile("Config.json"),sessionFile("Session.json");
    try {
        TestWorkerExchange();
        Check(BatteryBandForPercent(-1)==BatteryLightbarBand::Unavailable,"unknown battery stays unknown");
        Check(BatteryBandForPercent(0)==BatteryLightbarBand::Red && BatteryBandForPercent(29)==BatteryLightbarBand::Red,"red battery threshold");
        Check(BatteryBandForPercent(30)==BatteryLightbarBand::Yellow && BatteryBandForPercent(69)==BatteryLightbarBand::Yellow,"yellow battery threshold");
        Check(BatteryBandForPercent(70)==BatteryLightbarBand::Green && BatteryBandForPercent(100)==BatteryLightbarBand::Green,"green battery threshold");
        for(int i=0;i<5;++i)Check(NearestLightbarCycleSpeed(LightbarCycleSpeeds[i])==i,"cycle speed mapping");
        for(auto base:{BaseLightbarMode::Static,BaseLightbarMode::ColorCycle,BaseLightbarMode::Battery})
            for(bool enabled:{false,true})for(bool available:{false,true}) {
                const auto expected=enabled&&available?LightbarMode::Game:base==BaseLightbarMode::Battery?LightbarMode::Battery:base==BaseLightbarMode::ColorCycle?LightbarMode::ColorCycle:LightbarMode::Static;
                Check(EffectiveLightbarMode(base,enabled,available)==expected,"game override requires actual feedback and overrides every base mode");
            }
        UiText::Dictionary dictionary{{"@Label@","Etiket"},{"@State@","Hazır"}};
        Check(UiText::Resolve(dictionary,"@Label@: 42 / @State@") == "Etiket: 42 / Hazır","resource templates retain live values");
        Check(UiText::Resolve(dictionary,"@Unknown@") == "@Unknown@","unknown tokens preserved");
        ApplicationCore core;
        Check(!core.GetSnapshot()->running,"initial snapshot");
        Check(core.SelectController(0)==0,"reject before start");
        Check(core.Start()==ApplicationCore::StartResult::Running,"close DualsenseZ before running this test");
        { ApplicationCore competing;bool rejected=false;
          try { competing.Start(); } catch(const std::logic_error&) {rejected=true;}
          Check(rejected,"overlapping process-global backends rejected"); }
        core.Tick();
        // Focused lighting verification also works while a physical controller is attached.
        if(argc>1&&std::string(argv[1])=="--lighting") {
            const int slot=3;
            core.SetLightbarColor(slot,{0.2f,0.4f,0.6f});core.SetLightbarBrightness(slot,0.75f);
            core.SetFeature(slot,FeatureOption::DiscoSpeed,0.25);core.SetFeature(slot,FeatureOption::AudioToLed,1);
            for(auto base:{BaseLightbarMode::Battery,BaseLightbarMode::ColorCycle,BaseLightbarMode::Static})for(bool game:{true,false}) {
                core.SetBaseLightbarMode(slot,base);core.SetFeature(slot,FeatureOption::EmulatedLight,game?1:0);
                core.SetPlayerLedEnabled(slot,true);core.Tick();
                for(auto& result:core.DrainResults())Check(result.status==ApplicationCore::CommandStatus::Succeeded,"lighting command succeeds");
                const auto& c=core.GetSnapshot()->controllers[slot];
                Check(c.baseLightbarMode==base,"exclusive base snapshot");
                Check(c.batteryBasedLightbarEnabled==(base==BaseLightbarMode::Battery)&&bool(c.features[size_t(FeatureOption::Disco)])==(base==BaseLightbarMode::ColorCycle),"one automatic mode at most");
                Check(c.lightbarMode==EffectiveLightbarMode(base,game,false),"game toggle alone does not replace base color");
                Check(c.gameLightbarOverrideEnabled==game,"override remains independent");
                Check(c.lightbarColor==std::array<float,3>{0.2f,0.4f,0.6f}&&c.lightbarBrightness==0.75f&&c.features[size_t(FeatureOption::DiscoSpeed)]==0.25,"inactive mode settings preserved");
                Check(c.playerLedEnabled&&c.features[size_t(FeatureOption::AudioToLed)]==1,"sound and player LEDs remain independent");
            }
            core.Stop();Check(!core.GetSnapshot()->running,"lighting clean shutdown");
            std::cout<<"Lighting tests passed\n";return 0;
        }
        Check(core.GetSnapshot()->connectedCount==0,"disconnect physical controllers before this test");
        const auto initial=core.GetSnapshot();
        for(auto language:{"en","tr","en"}) {
            auto preferences=initial->preferences;preferences.language=language;
            Check(core.SetPreferences(preferences)!=0,"language preference queued");core.Tick();core.DrainResults();
            auto localized=core.GetSnapshot();
            Check(localized->uiText&&UiText::Resolve(*localized->uiText,"@WinUI.SelectedController@")==(std::string(language)=="tr"?"Seçili kontrolcü":"Selected controller"),"language snapshot resource refresh");
        }
        core.SetPreferences(initial->preferences);core.Tick();core.DrainResults();
        Check(!core.SetFeature(0,FeatureOption::Count,0),"feature enum validation");
        Check(!core.SetFeature(0,FeatureOption::Speaker,9),"feature range validation");
        Check(!core.SetFeature(0,FeatureOption::Speaker,0.5),"integer feature validation");
        Check(!core.SetFeature(0,FeatureOption::HapticIntensity,std::numeric_limits<double>::quiet_NaN()),"finite feature validation");
        for(auto& d:FeatureDescriptors())Check(core.SetFeature(1,d.id,d.maximum)!=0,"all feature commands accepted");
        Check(core.SetHotkey(1,HotkeyTarget::GyroStick,0x100)!=0,"hotkey command");
        core.Tick();
        for(auto& r:core.DrainResults())Check(r.status==ApplicationCore::CommandStatus::Succeeded,"feature command execution");
        for(auto& d:FeatureDescriptors()) {
            Check(std::abs(core.GetSnapshot()->controllers[1].features[size_t(d.id)]-d.maximum)<0.00001,"feature snapshot mapping");
            Check(core.GetSnapshot()->controllers[0].features[size_t(d.id)]==initial->controllers[0].features[size_t(d.id)],"feature slot isolation");
            core.SetFeature(1,d.id,initial->controllers[1].features[size_t(d.id)]);
        }
        core.SetHotkey(1,HotkeyTarget::GyroStick,initial->controllers[1].hotkeys[0]);core.Tick();core.DrainResults();
        for(bool dsx:{false,true})for(int mode=0;mode<int(TriggerModes(dsx).size());++mode) {
            std::array<int,11> values{};const auto& schema=TriggerModes(dsx)[mode];
            for(size_t k=0;k<schema.parameters.size();++k)values[k]=schema.parameters[k].minimum;
            if(schema.parameters.size()>1&&schema.parameters[1].name.find("End")!=std::string::npos)values[1]=std::max(values[1],values[0]+1);
            Check(core.SetTrigger(1,0,dsx,mode,values)!=0,"trigger catalog command validation");
            core.Tick();auto results=core.DrainResults();
            Check(results.size()==1&&results[0].status==ApplicationCore::CommandStatus::Succeeded,"trigger command completion");
            Check(core.GetSnapshot()->controllers[1].triggers[0].mode==schema.name,"trigger catalog/backend order");
        }
        Check(!core.SetTrigger(0,2,false,0,{}),"invalid trigger side");
        Check(!core.SetTrigger(0,0,false,2,{7,3,1}),"weapon position ordering");
        Check(!core.SetTrigger(0,0,true,14,{0,1,6,0,10}),"galloping foot ordering");
        
        core.SetTrigger(1,0,false,0,{});core.Tick();core.DrainResults();
        Check(!core.SelectController(-1)&&!core.SelectController(4),"slot validation");
        Check(!core.SetLightbarColor(0,{std::numeric_limits<float>::quiet_NaN(),0,0}),"color validation");
        Check(core.SetLightbarColor(0,{0.1f,0.2f,0.3f})!=0,"queue color");
        Check(!core.SetLightbarBrightness(0,-0.1f)&&!core.SetLightbarBrightness(0,1.1f)&&
            !core.SetLightbarBrightness(0,std::numeric_limits<float>::quiet_NaN()),"global brightness validation");
        Check(core.SetLightbarBrightness(0,0.75f)!=0,"queue global brightness");
        Check(core.SetPlayerLedEnabled(0,false)!=0,"queue LED");
        Check(core.SetBatteryBasedLightbarEnabled(0,true)!=0,"queue battery LED preference");
        const auto beforeProfile=core.GetSnapshot();
        auto path=(std::filesystem::current_path()/"core-boundary-profile.dsy").string();
        auto save=core.SaveProfile(0,path);
        Check(core.GetSnapshot()->revision==beforeProfile->revision,"reads don't process commands");
        core.Tick();
        Check(initial->controllers[0].lightbarColor!=core.GetSnapshot()->controllers[0].lightbarColor,"old snapshot immutable");
        bool saved=false;
        for(auto& r:core.DrainResults()) if(r.id==save) saved=r.status==ApplicationCore::CommandStatus::Succeeded;
        Check(saved,"save result");
        Check(core.GetSnapshot()->controllers[0].batteryBasedLightbarEnabled,"battery LED snapshot");
        Check(core.GetSnapshot()->controllers[0].lightbarMode==LightbarMode::Battery,"battery owns lightbar snapshot");
        Check(core.GetSnapshot()->controllers[0].lightbarBrightness==0.75f,"global brightness snapshot");
        core.SetLightbarColor(0,{1,1,1});
        core.SetLightbarBrightness(0,0.2f);
        core.SetBatteryBasedLightbarEnabled(0,false);
        auto load=core.LoadProfile(0,path);
        core.Tick();
        bool loaded=false;
        for(auto& r:core.DrainResults()) if(r.id==load) loaded=r.status==ApplicationCore::CommandStatus::Succeeded;
        Check(loaded&&core.GetSnapshot()->controllers[0].lightbarColor[0]==0.1f,"FIFO profile roundtrip");
        Check(core.GetSnapshot()->controllers[0].batteryBasedLightbarEnabled,"battery LED profile roundtrip");
        Check(core.GetSnapshot()->controllers[0].lightbarBrightness==0.75f,"global brightness profile roundtrip");
        for(auto base:{BaseLightbarMode::Static,BaseLightbarMode::ColorCycle,BaseLightbarMode::Battery})for(bool game:{false,true}) {
            Check(core.SetBaseLightbarMode(0,base)!=0,"exclusive mode command");
            core.SetFeature(0,FeatureOption::EmulatedLight,game?1:0);
            core.SetPlayerLedEnabled(0,true);
            core.Tick();core.DrainResults();
            const auto& c=core.GetSnapshot()->controllers[0];
            Check(c.baseLightbarMode==base,"selected base snapshot");
            Check(c.batteryBasedLightbarEnabled==(base==BaseLightbarMode::Battery)&&bool(c.features[size_t(FeatureOption::Disco)])==(base==BaseLightbarMode::ColorCycle),"atomic exclusive automatic flags");
            Check(c.lightbarMode==EffectiveLightbarMode(base,game,false),"no game feedback falls back to selected base");
            Check(c.gameLightbarOverrideEnabled==game,"game preference independent from base");
            Check(c.lightbarBrightness==0.75f&&c.lightbarColor[0]==0.1f,"mode changes preserve brightness and static color");
            Check(c.playerLedEnabled,"player LEDs independent of color mode");
        }
        core.SetBatteryBasedLightbarEnabled(0,true);core.SetFeature(0,FeatureOption::Disco,1);core.Tick();core.DrainResults();
        Check(core.GetSnapshot()->controllers[0].baseLightbarMode==BaseLightbarMode::ColorCycle&&!core.GetSnapshot()->controllers[0].batteryBasedLightbarEnabled,"legacy cycle command selects exclusively");
        core.SetBatteryBasedLightbarEnabled(0,true);core.Tick();core.DrainResults();
        Check(core.GetSnapshot()->controllers[0].features[size_t(FeatureOption::Disco)]==0,"legacy battery command selects exclusively");
        Check(!core.SetBaseLightbarMode(0,static_cast<BaseLightbarMode>(99)),"base enum validation");
        auto bad=core.LoadProfile(0,path+".missing"); core.Tick();
        auto results=core.DrainResults();
        Check(results.size()==1&&results[0].id==bad&&results[0].status==ApplicationCore::CommandStatus::Failed,"load failure");
        Check(!core.SetControllerOption(4,ControllerOption::LeftDeadzone,1),"controller option slot validation");
        Check(!core.SetControllerOption(0,ControllerOption::LeftDeadzone,128),"deadzone range validation");
        Check(!core.SetControllerOption(0,ControllerOption::ShareAsSelect,2),"boolean option validation");
        Check(!core.SetControllerOption(0,static_cast<ControllerOption>(99),0),"option enum validation");
        core.SetControllerOption(2,ControllerOption::LeftDeadzone,17);
        core.SetControllerOption(2,ControllerOption::RightDeadzone,31);
        core.SetControllerOption(2,ControllerOption::ShareAsSelect,1);
        core.SetControllerOption(2,ControllerOption::AnalogWasd,1);
        core.SetControllerOption(2,ControllerOption::PsAsWindows,1);
        core.SelectController(2);core.Tick();
        auto controllerState=core.GetSnapshot();
        const auto& controller=controllerState->controllers[2];
        Check(controllerState->selectedController==2&&controller.leftDeadzone==17&&controller.rightDeadzone==31&&controller.shareAsSelect&&controller.analogWasd&&controller.psAsWindows,"controller option commands and selection");
        Check(controllerState->controllers[0].leftDeadzone==0,"controller options are slot scoped");
        Check(!controller.connected&&controller.stickAxes[0]==128&&!controller.leftStickPressed,"disconnected telemetry neutral");
        core.DrainResults();
        core.SelectController(0);core.Tick();core.SelectController(2);core.Tick();core.DrainResults();
        Check(core.GetSnapshot()->controllers[2].leftDeadzone==17,"selection retains controller configuration");
        auto hide=core.SetControllerHidden(2,true);core.Tick();results=core.DrainResults();
        Check(results.size()==1&&results[0].id==hide&&results[0].status==ApplicationCore::CommandStatus::Failed,"unavailable hiding produces failure");
        std::atomic<bool> done{false},valid{true},produced{false};
        std::thread reader([&]{std::uint64_t previous=0;while(!done){auto s=core.GetSnapshot();if(s->revision<previous)valid=false;previous=s->revision;}});
        std::thread producer([&]{for(int i=0;i<100;++i) {if(!core.SelectController(i%4))valid=false;if(!core.SetControllerOption(i%4,ControllerOption::LeftDeadzone,i%128))valid=false;}produced=true;});
        do {core.Tick();} while(!produced);
        producer.join();
        for(int i=0;i<20;++i) core.Tick();
        done=true;reader.join();
        Check(valid,"concurrent submission and snapshot reads");
        Check(core.DrainResults().size()==200,"all accepted commands complete");
        for(int i=0;i<256;++i) {
            auto id=i%3==0?core.SaveProfile(0,path):i%3==1?core.SetControllerHidden(2,true):core.SelectController(i%4);
            Check(id!=0,"queue capacity");
        }
        Check(!core.SelectController(0),"bounded queue rejects overflow");
        core.Stop();
        results=core.DrainResults();Check(results.size()==256,"stop completes pending commands");
        for(auto& r:results) Check(r.status==ApplicationCore::CommandStatus::Cancelled,"cancel status");
        Check(!core.GetSnapshot()->running&&!core.SelectController(0),"stopped boundary");
        core.Stop();
        Check(initial->running,"snapshot survives stop");
        for(int session=0;session<4;++session) {
            ApplicationCore next;
            Check(next.Start()==ApplicationCore::StartResult::Running,"repeated runtime session");
            auto preferences=next.GetSnapshot()->preferences;
            for(int n=0;n<20;++n) {
                auto changed=preferences;
                changed.hideToTrayOnMinimize=preferences.hideToTrayOnMinimize^(n%2==0);
                Check(next.SetPreferences(changed)!=0,"rapid preference commands");
            }
            next.Tick();
            auto completed=next.DrainResults();
            Check(completed.size()==20,"preference completion count");
            for(const auto& r:completed) Check(r.status==ApplicationCore::CommandStatus::Succeeded,"preference completion");
            auto held=next.GetSnapshot();
            std::atomic<bool> stopping=false;
            std::thread concurrent([&]{while(!stopping) {next.GetSnapshot();next.SelectController(0);}});
            next.Stop();stopping=true;concurrent.join();next.DrainResults();
            Check(held->running&&!next.GetSnapshot()->running,"session snapshot lifetime");
        }
        std::cout<<"PASS: worker exchanges, overlapping-session rejection, repeated sessions, rapid options/preferences, queued profile/HidHide cancellation;  snapshots, FIFO, profile roundtrip, errors, concurrent readers/producers, backpressure, cancellation, shutdown\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

