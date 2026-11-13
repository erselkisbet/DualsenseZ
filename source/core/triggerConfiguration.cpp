#include "core/triggerConfiguration.hpp"
void ApplyTriggerParameters(s_scePadSettings& settings)
{
    for (int i = 0; i < TRIGGER_COUNT; ++i) {
        settings.currentSonyItem[i] = std::clamp(settings.currentSonyItem[i], 0, int(sonyItems.size())-1);
        settings.currentDSXItem[i] = std::clamp(settings.currentDSXItem[i], 0, int(dsxItems.size())-1);
        settings.uiTriggerFormat[i] = settings.uiTriggerFormat[i] == DSX_FORMAT ? DSX_FORMAT : SONY_FORMAT;
        settings.uiSelectedSonyTriggerMode[i] = sonyItems[settings.currentSonyItem[i]];
        settings.uiSelectedDSXTriggerMode[i] = dsxItems[settings.currentDSXItem[i]];
    }
    settings.uiSelectedTrigger = std::clamp(settings.uiSelectedTrigger, 0, 1);
	// Apply triggers from UI
	for (int i = 0; i < TRIGGER_COUNT; i++)
	{
		std::vector<uint8_t> vec;

		for (int j = 0; j < MAX_PARAM_COUNT; j++)
		{
			vec.push_back(settings.uiParameters[i][j]);
		}

		if (settings.uiTriggerFormat[i] == SONY_FORMAT)
		{
			if (auto it = sonyTriggerHandlers.find(sonyItems[settings.currentSonyItem[i]]); it != sonyTriggerHandlers.end())
				it->second(settings, i, vec);
		}
		else
		{
			if (auto it = dsxTriggerHandlers.find(dsxItems[settings.currentDSXItem[i]]); it != dsxTriggerHandlers.end())
				it->second(settings, i, vec);
		}
	}

}

