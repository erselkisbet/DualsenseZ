#pragma once
#include "core/applicationCore.hpp"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <functional>
#include <vector>

// UI-thread callbacks into the shell. Results remain owned by ApplicationCore.
struct NotificationClient {
    using Severity=winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity;
    std::function<void(winrt::hstring const&,Severity)> show;
    std::function<void(std::vector<ApplicationCore::RequestId> const&,winrt::hstring const&,Severity)> track;
    void Show(winrt::hstring const& message,Severity severity) const {if(show)show(message,severity);}
    void Submit(ApplicationCore::RequestId id,winrt::hstring const& success=L"@WinUI.ChangesApplied@",Severity severity=Severity::Success) const {
        SubmitBatch({id},success,severity);
    }
    void SubmitBatch(std::vector<ApplicationCore::RequestId> const& ids,winrt::hstring const& success=L"@WinUI.ChangesApplied@",Severity severity=Severity::Success) const {
        if(track)track(ids,success,severity);
    }
};
