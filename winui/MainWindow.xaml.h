#pragma once
#include "MainWindow.g.h"
#include "Services/CoreHost.h"
#include "Services/Notifications.h"
#include <map>
#include <commctrl.h>
#include "Services/Localization.h"
namespace winrt::DualsenseZNative::implementation {
struct MainWindow : MainWindowT<MainWindow> {
    MainWindow();
    ~MainWindow();
    void Attach(std::shared_ptr<CoreHost> host);
    void Navigate(Microsoft::UI::Xaml::Controls::NavigationView const&,Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);
private:
    NativeLocalization::Translator m_translator;
    void Refresh();
    NotificationClient Notifications();
    void ShowNotification(winrt::hstring const&,Microsoft::UI::Xaml::Controls::InfoBarSeverity);
    void TrackNotifications(std::vector<ApplicationCore::RequestId> const&,winrt::hstring const&,Microsoft::UI::Xaml::Controls::InfoBarSeverity);
    void NotifyCompleted(ApplicationCore::CommandResult const&);
    struct PendingNotification {
        size_t remaining=0;
        bool failed=false;
        winrt::hstring success;
        Microsoft::UI::Xaml::Controls::InfoBarSeverity severity;
    };
    std::map<ApplicationCore::RequestId,std::shared_ptr<PendingNotification>> m_notifications;
    Microsoft::UI::Dispatching::DispatcherQueueTimer m_notificationTimer{nullptr};
    void AddTray();
    void ShowFromTray();
    void VisibilityChanged(bool visible);
    void CloseHost();
    void UpdateTitleTheme();
    void UpdateWindowIcons(UINT dpi);
    winrt::fire_and_forget CheckForUpdate();
    void ShowUpdate(std::string const& version,winrt::hstring const& page);
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    std::shared_ptr<CoreHost> m_host;
    Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{nullptr};
    winrt::event_token m_timerToken{};
    CoreHost::State m_lastState=CoreHost::State::Starting;
    HWND m_hwnd=nullptr;
    HICON m_appIcon=nullptr,m_titleIcon=nullptr;
    bool m_closed=false,m_hideOnMinimize=false,m_hideOnClose=false,m_exitRequested=false,m_startBehaviorApplied=false,m_trayAdded=false,m_updateChecked=false;
    Microsoft::UI::Xaml::Controls::Page m_settingsPage{nullptr};
};
}
namespace winrt::DualsenseZNative::factory_implementation {
struct MainWindow : MainWindowT<MainWindow,implementation::MainWindow> {};
}

