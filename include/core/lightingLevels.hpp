#pragma once
#include <array>
#include <cmath>

enum class LightbarMode { Static, Game, ColorCycle, Battery, None };
enum class BaseLightbarMode { Static, ColorCycle, Battery, None };
constexpr BaseLightbarMode SelectedBaseLightbarMode(bool battery, bool cycle, bool staticEnabled=true) {
    return battery ? BaseLightbarMode::Battery : cycle ? BaseLightbarMode::ColorCycle : staticEnabled ? BaseLightbarMode::Static : BaseLightbarMode::None;
}
constexpr LightbarMode EffectiveLightbarMode(BaseLightbarMode base, bool gameEnabled, bool gameAvailable) {
    return gameEnabled && gameAvailable ? LightbarMode::Game :
        base==BaseLightbarMode::Battery ? LightbarMode::Battery :
        base==BaseLightbarMode::ColorCycle ? LightbarMode::ColorCycle : base==BaseLightbarMode::Static ? LightbarMode::Static : LightbarMode::None;
}

constexpr std::array<double,5> LightbarCycleSpeeds{0.03,0.06,0.10,0.25,0.50};
inline int NearestLightbarCycleSpeed(double value) {
    int best=0;
    for(int i=1;i<5;++i)
        if(std::abs(value-LightbarCycleSpeeds[i])<std::abs(value-LightbarCycleSpeeds[best]))best=i;
    return best;
}
