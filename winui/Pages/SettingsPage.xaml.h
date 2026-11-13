#pragma once
#include "SettingsPage.g.h"
#include "Services/CoreHost.h"
#include "Services/Notifications.h"
namespace winrt::DualsenseZNative::implementation {
struct SettingsPage : SettingsPageT<SettingsPage> {
    SettingsPage();
    void Attach(std::shared_ptr<CoreHost> host,NotificationClient notifications) {m_notifications=std::move(notifications);m_host=std::move(host);m_pending=0;}
    void Update(ApplicationSnapshot const& snapshot);
    void Completed(ApplicationCore::CommandResult const& result);
    void LanguageChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void TrayModeChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
    void PreferencesChanged(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
    void AssociateClicked(Windows::Foundation::IInspectable const&,Microsoft::UI::Xaml::RoutedEventArgs const&);
private:
    void Submit();
    std::shared_ptr<CoreHost> m_host;
    NotificationClient m_notifications;
    ApplicationPreferences m_draft;
    ApplicationCore::RequestId m_pending=0;
    bool m_updating=true;
};
}
namespace winrt::DualsenseZNative::factory_implementation {
struct SettingsPage : SettingsPageT<SettingsPage,implementation::SettingsPage> {};
}
