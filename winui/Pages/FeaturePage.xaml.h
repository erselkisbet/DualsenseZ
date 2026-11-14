#pragma once
#include "FeaturePage.g.h"
#include "Services/CoreHost.h"
#include "Services/Notifications.h"
#include <functional>
#include <map>
#include <limits>
#include <set>
namespace winrt::DualsenseZNative::implementation {
struct FeaturePage : FeaturePageT<FeaturePage> {
    FeaturePage();
    void Attach(std::shared_ptr<CoreHost> host,HWND window,NotificationClient notifications) {m_notifications=std::move(notifications);m_host=std::move(host);m_window=window;}
    void OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&);
    void Update(ApplicationSnapshot const&);
    void Suspend() {m_capture=-1;}
private:
    using Panel=Microsoft::UI::Xaml::Controls::StackPanel;
    struct TriggerEditor {
        Microsoft::UI::Xaml::Controls::ComboBox format,mode;
        Panel parameters;
        std::vector<Microsoft::UI::Xaml::Controls::NumberBox> numbers;
        TriggerSnapshot last;
        bool initialized=false;
    };
    struct View {
        Microsoft::UI::Xaml::Controls::ScrollViewer root;
        Panel content;
        Microsoft::UI::Xaml::Controls::Grid dashboard;
        Panel left,right;
        Microsoft::UI::Xaml::Controls::ComboBox selector{nullptr};
        std::vector<std::function<void(ApplicationSnapshot const&,bool)>> bindings;
        std::array<TriggerEditor,2> triggers;
        int slot=-1;
    };
    std::shared_ptr<View> Build(std::string const& name);
    Panel Card(Microsoft::UI::Xaml::Controls::Panel const&,std::string const& title);
    Microsoft::UI::Xaml::Controls::Button Action(std::string const&,std::function<void(FeaturePage&)> action);
    void Button(Panel const&,std::string const&,std::function<void(FeaturePage&)> action);
    void Options(std::shared_ptr<View> const&,std::string const& section);
    void LightingOptions(std::shared_ptr<View> const&);
    void Triggers(std::shared_ptr<View> const&);
    void TriggerBehavior(std::shared_ptr<View> const&);
    void MotionTelemetry(std::shared_ptr<View> const&);
    void DeadzoneTelemetry(std::shared_ptr<View> const&);
    void TriggerPresets(std::shared_ptr<View> const&);
    void RebuildTrigger(TriggerEditor&,bool defaults);
    void UdpStatus(std::shared_ptr<View> const&);
    void Profiles(std::shared_ptr<View> const&);
    Microsoft::UI::Xaml::FrameworkElement GameCard(GameProfileEntry const&,bool active,bool enabled);
    Microsoft::UI::Xaml::FrameworkElement AddGameCard(bool enabled);
    winrt::fire_and_forget PickProfile(int operation);
    winrt::fire_and_forget AddGameDialog();
    winrt::fire_and_forget FetchCover(GameProfileEntry game);
    void Telemetry(std::shared_ptr<View> const&);
    void Submit(ApplicationCore::RequestId);
    std::shared_ptr<CoreHost> m_host;
    NotificationClient m_notifications;
    HWND m_window=nullptr;
    bool m_updating=false;
    int m_selected=0,m_capture=-1,m_captureSlot=0;
    std::chrono::steady_clock::time_point m_captureEnd;
    std::map<std::string,std::shared_ptr<View>> m_views;
    std::shared_ptr<View> m_view;
    std::string m_name;
    std::set<std::string> m_coverAttempts; // one cover download per game per run
};
}
namespace winrt::DualsenseZNative::factory_implementation {
struct FeaturePage : FeaturePageT<FeaturePage,implementation::FeaturePage> {};
}
