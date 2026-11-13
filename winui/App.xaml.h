#pragma once
#include "App.xaml.g.h"
#include "Services/CoreHost.h"
namespace winrt::DualsenseZNative::implementation {
struct App : AppT<App> {
    App();
    ~App();
    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
private:
    std::shared_ptr<CoreHost> m_host;
    Microsoft::UI::Xaml::Window m_window{nullptr};
};
}
