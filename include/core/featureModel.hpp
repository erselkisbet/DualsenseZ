#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
enum class FeatureOption {
#define OPTION(id,field,label,section,min,max,step) id,
#include "featureOptions.inc"
#undef OPTION
    Count
};
struct FeatureDescriptor {FeatureOption id;const char* label;const char* section;double minimum,maximum,step;};
inline const std::vector<FeatureDescriptor>& FeatureDescriptors() {
    static const std::vector<FeatureDescriptor> values={
#define OPTION(id,field,label,section,min,max,step) {FeatureOption::id,label,section,min,max,step},
#include "featureOptions.inc"
#undef OPTION
    };return values;
}
enum class HotkeyTarget {GyroStick, GyroMouse, MouseClick};
struct TriggerParameter {std::string name;int minimum=0,maximum=255;};
struct TriggerModeInfo {std::string name;std::vector<TriggerParameter> parameters;};
const std::vector<TriggerModeInfo>& TriggerModes(bool dsx);
struct TouchSnapshot {bool active=false;int x=0,y=0,id=0;};
