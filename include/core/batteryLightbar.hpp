#pragma once
#include <array>

enum class BatteryLightbarBand { Unavailable=-1, Red=0, Yellow=1, Green=2 };
constexpr BatteryLightbarBand BatteryBandForPercent(int percent) {
    return percent<0 || percent>100 ? BatteryLightbarBand::Unavailable :
        percent>=70 ? BatteryLightbarBand::Green :
        percent>=30 ? BatteryLightbarBand::Yellow : BatteryLightbarBand::Red;
}
constexpr std::array<float,3> BatteryBandColor(BatteryLightbarBand band) {
    switch(band) {
    case BatteryLightbarBand::Green: return {34.f/255.f,197.f/255.f,94.f/255.f};
    case BatteryLightbarBand::Yellow: return {250.f/255.f,204.f/255.f,21.f/255.f};
    case BatteryLightbarBand::Red: return {239.f/255.f,68.f/255.f,68.f/255.f};
    default: return {0,0,0};
    }
}
