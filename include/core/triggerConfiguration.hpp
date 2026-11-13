#pragma once
#include "scePadSettings.hpp"
inline const std::vector<std::string> sonyItems = {TriggerStringSony::OFF, TriggerStringSony::FEEDBACK, TriggerStringSony::WEAPON, TriggerStringSony::VIBRATION, TriggerStringSony::SLOPE_FEEDBACK, TriggerStringSony::MULTIPLE_POSITION_FEEDBACK, TriggerStringSony::MULTIPLE_POSITION_VIBRATION};
inline const std::vector<std::string> dsxItems = {TriggerStringDSX::Normal, TriggerStringDSX::GameCube, TriggerStringDSX::VerySoft, TriggerStringDSX::Soft, TriggerStringDSX::Medium, TriggerStringDSX::Hard, TriggerStringDSX::VeryHard, TriggerStringDSX::Hardest, TriggerStringDSX::VibrateTrigger, TriggerStringDSX::VibrateTriggerPulse, TriggerStringDSX::Choppy, TriggerStringDSX::CustomTriggerValue, TriggerStringDSX::Resistance, TriggerStringDSX::Bow, TriggerStringDSX::Galloping, TriggerStringDSX::SemiAutomaticGun, TriggerStringDSX::AutomaticGun, TriggerStringDSX::Machine, TriggerStringDSX::VIBRATE_TRIGGER_10Hz};

void ApplyTriggerParameters(s_scePadSettings& settings);
