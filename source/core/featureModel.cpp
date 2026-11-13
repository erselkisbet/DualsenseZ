#include "core/featureModel.hpp"
const std::vector<TriggerModeInfo>& TriggerModes(bool dsx) {
    static const auto sony=[] {
        std::vector<TriggerModeInfo> modes={
            {"Off",{}},{"Feedback",{{"Position",0,9},{"Strength",1,8}}},
            {"Weapon",{{"Start position",2,7},{"End position",3,8},{"Strength",1,8}}},
            {"Vibration",{{"Position",0,9},{"Amplitude",1,8},{"Frequency",1,255}}},
            {"Slope Feedback",{{"Start position",1,8},{"End position",2,9},{"Start strength",1,8},{"End strength",1,8}}},
            {"Multiple Position Feedback",{}},{"Multiple Position Vibration",{{"Frequency",0,255}}}};
        for(int i=0;i<10;++i) {
            modes[5].parameters.push_back({"Strength at position "+std::to_string(i),0,8});
            modes[6].parameters.push_back({"Amplitude at position "+std::to_string(i),0,8});
        }return modes;
    }();
    static const auto custom=[] {
        std::vector<TriggerModeInfo> modes;
        for(auto name:{"Normal","GameCube","VerySoft","Soft","Medium","Hard","VeryHard","Hardest","VibrateTrigger","VibrateTriggerPulse","Choppy"})modes.push_back({name,{}});
        modes.push_back({"CustomTriggerValue",{{"Mode: 0 Off, 1 Rigid, 2 Rigid_A, 3 Rigid_B, 4 Rigid_AB, 5 Pulse, 6 Pulse_A, 7 Pulse_B, 8 Pulse_AB",0,8}}});
        for(int i=1;i<11;++i)modes.back().parameters.push_back({"Parameter "+std::to_string(i),0,255});
        modes.push_back({"Resistance",{{"Start",0,9},{"Force",0,8}}});
        modes.push_back({"Bow",{{"Start",0,7},{"End",1,8},{"Force",0,8},{"Snap force",0,8}}});
        modes.push_back({"Galloping",{{"Start",0,8},{"End",1,9},{"First foot",0,6},{"Second foot",0,6},{"Frequency",0,255}}});
        modes.push_back({"SemiAutomaticGun",{{"Start",2,7},{"End",2,8},{"Force",1,8}}});
        modes.push_back({"AutomaticGun",{{"Start",0,9},{"Strength",1,8},{"Frequency",1,255}}});
        modes.push_back({"Machine",{{"Start",0,8},{"End",0,9},{"Strength A",0,7},{"Strength B",0,7},{"Frequency",0,255},{"Period",0,255}}});
        modes.push_back({"VIBRATE_TRIGGER_10Hz",{}});return modes;
    }();
    return dsx?custom:sony;
}
