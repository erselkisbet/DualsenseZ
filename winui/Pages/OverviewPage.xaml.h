#pragma once
#include "OverviewPage.g.h"
#include "Services/CoreHost.h"
#include "Services/Notifications.h"
#include <functional>
namespace winrt::DualsenseZNative::implementation {
struct OverviewPage : OverviewPageT<OverviewPage> {
    OverviewPage();
    void Attach(std::shared_ptr<CoreHost> host, std::function<void(winrt::hstring const&)> navigate,NotificationClient notifications) {m_notifications=std::move(notifications);m_host=std::move(host);m_navigate=std::move(navigate);}
    void Update(ApplicationSnapshot const& snapshot);
    void SelectController(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void SelectEmulation(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OpenPage(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void LayoutChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::SizeChangedEventArgs const&);
private:
    void Submitted(ApplicationCore::RequestId id);
    std::shared_ptr<CoreHost> m_host;
    NotificationClient m_notifications;
    std::function<void(winrt::hstring const&)> m_navigate;
    bool m_updating=false;
    bool m_hasConnectionState=false;
    bool m_wasConnected=false;
    int m_batteryWarning=0;
    int m_selected=0;
};
}
namespace winrt::DualsenseZNative::factory_implementation {
struct OverviewPage : OverviewPageT<OverviewPage,implementation::OverviewPage> {};
}


