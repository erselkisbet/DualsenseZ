#pragma once
#include "ControllerPage.g.h"
#include "Services/CoreHost.h"
#include "Services/Notifications.h"
#include <functional>
namespace winrt::DualsenseZNative::implementation {
struct ControllerPage : ControllerPageT<ControllerPage> {
    ControllerPage();
    void Attach(std::shared_ptr<CoreHost> host,HWND window,std::function<void()> openSettings,NotificationClient notifications) {m_notifications=std::move(notifications);m_host=std::move(host);m_window=window;m_openSettings=std::move(openSettings);}
    void Update(ApplicationSnapshot const& snapshot);
    void SelectController(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void OptionChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void DeadzoneChanged(Microsoft::UI::Xaml::Controls::NumberBox const&,Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const&);
    void DeadzoneSliderChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const&);
    void OpenSettingsClicked(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
private:
    void Submitted(ApplicationCore::RequestId id);
    void ArrangeVisibility();
    std::shared_ptr<CoreHost> m_host;
    NotificationClient m_notifications;
    std::function<void()> m_openSettings;
    HWND m_window=nullptr;
    bool m_updating=true;
    int m_selected=0;
};
}
namespace winrt::DualsenseZNative::factory_implementation {
struct ControllerPage : ControllerPageT<ControllerPage,implementation::ControllerPage> {};
}
