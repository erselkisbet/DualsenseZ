#include "pch.h"
#include "Services/Localization.h"
#include "OverviewPage.xaml.h"
#include "ControllerPageHeader.h"
#include "OverviewPage.xaml.g.hpp"
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#if __has_include("OverviewPage.g.cpp")
#include "OverviewPage.g.cpp"
#endif
#include <filesystem>
#include <algorithm>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace winrt::DualsenseZNative::implementation {
OverviewPage::OverviewPage() {
    InitializeComponent();
    ControllerPageHeader::Attach(PageHeader());
    NavigationCacheMode(Navigation::NavigationCacheMode::Required);
    auto fade=Resources().Lookup(box_value(L"ControllerFadeIn")).as<Media::Animation::Storyboard>();
    Media::Animation::Storyboard::SetTarget(fade.Children().GetAt(0),ConnectedPreview());
}
void OverviewPage::Update(ApplicationSnapshot const& s) {
    m_updating=true;
    m_selected=s.selectedController;const auto& c=s.controllers[m_selected];
    auto text=[](TextBlock const& block,hstring const& value){auto translated=NativeLocalization::Text(value);if(block.Text()!=translated)block.Text(translated);};
    if(ControllerSelector().SelectedIndex()!=m_selected)ControllerSelector().SelectedIndex(m_selected);
    ControllerSelector().IsEnabled(s.running);
    auto device=c.connected?to_hstring(c.name):hstring(L"@WinUI.NoControllerConnected@");
    auto connection=!c.connected?hstring(L"@Disconnected@"):c.connection==ControllerConnection::Bluetooth?hstring(L"Bluetooth"):hstring(L"USB");
    auto profile=c.profilePath.empty()?hstring(L"@CurrentSettings@"):to_hstring(std::filesystem::path(c.profilePath).stem().string());
    auto mode=c.emulation==EmulationMode::Xbox360?hstring(L"Xbox 360"):c.emulation==EmulationMode::DualShock4?hstring(L"DualShock 4"):NativeLocalization::Text(L"@None@");
    text(DeviceName(),device);text(ActiveDevice(),device);text(Connection(),connection);text(Profile(),profile);
    const bool hasBattery=c.connected&&c.batteryPercent>=0&&c.batteryPercent<=100;
    text(BatteryStatus(),hasBattery?to_hstring(c.batteryPercent)+L"%":hstring(L"\u2014"));
    // Segoe Fluent battery glyphs run from empty (E850) in ten steps; the full one is E83F.
    const int level=hasBattery?std::clamp((c.batteryPercent+5)/10,0,10):10;
    const wchar_t glyph[2]{level==10?wchar_t(0xE83F):wchar_t(0xE850+level),0};
    if(BatteryIcon().Glyph()!=glyph)BatteryIcon().Glyph(glyph);
    const int warning=!hasBattery?0:c.batteryPercent<=10?2:c.batteryPercent<=20?1:0;
    if(warning!=m_batteryWarning) {
        m_batteryWarning=warning;
        if(!warning){BatteryIcon().ClearValue(IconElement::ForegroundProperty());BatteryStatus().ClearValue(TextBlock::ForegroundProperty());}
        else {auto brush=Application::Current().Resources().Lookup(box_value(warning==2?L"SystemFillColorCriticalBrush":L"SystemFillColorCautionBrush")).as<Media::Brush>();BatteryIcon().Foreground(brush);BatteryStatus().Foreground(brush);}
    }    text(ActiveDetails(),NativeLocalization::Text(profile)+L" · "+NativeLocalization::Text(connection)+L" · "+mode);
    if(!m_hasConnectionState||m_wasConnected!=c.connected) {
        auto fade=Resources().Lookup(box_value(L"ControllerFadeIn")).as<Media::Animation::Storyboard>();
        fade.Stop();
        ConnectedPreview().Opacity(0);
        if(c.connected)fade.Begin();
        m_wasConnected=c.connected;
        m_hasConnectionState=true;
    }
    ConnectionIcon().Glyph(!c.connected?L"\xE8EA":c.connection==ControllerConnection::Bluetooth?L"\xE702":L"\xE88E");
    // Resolve theme brushes through the element's theme resources in XAML; opacity reinforces connection state.
    StatusIcon().Opacity(c.connected?1.0:0.35);
    EmulationNone().IsChecked(c.emulation==EmulationMode::None);
    EmulationXbox().IsChecked(c.emulation==EmulationMode::Xbox360);
    EmulationDS4().IsChecked(c.emulation==EmulationMode::DualShock4);
    const bool canEmulate=s.running&&c.connected&&s.emulationAvailable;
    EmulationNone().IsEnabled(canEmulate);EmulationXbox().IsEnabled(canEmulate);EmulationDS4().IsEnabled(canEmulate);
    text(EmulationStatus(),!c.connected?L"@WinUI.ConnectAControllerToApplyTheseSettingsToADevice@":!s.emulationAvailable?L"@WinUI.ViGEmIsUnavailable@":s.udpActive?L"@WinUI.AGameOrModIsControllingOutput@":L"@WinUI.ChooseHowGamesSeeYourController@");
    text(HeroStatus(),!c.connected?L"@WinUI.ConnectYourDualSenseOrDualShock4UsingUSBOrBluetooth@":s.udpActive?L"@WinUI.AGameOrModIsControllingOutput@":L"@WinUI.SettingsApplyToTheSelectedController@");
    m_updating=false;
}
void OverviewPage::Submitted(ApplicationCore::RequestId id) {m_notifications.Submit(id);}
void OverviewPage::SelectController(Windows::Foundation::IInspectable const&,SelectionChangedEventArgs const&) {
    if(!m_updating&&!NativeLocalization::applying&&m_host&&ControllerSelector().SelectedIndex()>=0)m_notifications.Submit(m_host->Core().SelectController(ControllerSelector().SelectedIndex()),L"");
}
void OverviewPage::SelectEmulation(Windows::Foundation::IInspectable const& sender,RoutedEventArgs const&) {
    if(!m_updating&&!NativeLocalization::applying&&m_host) {
        auto mode=std::stoi(to_string(unbox_value<hstring>(sender.as<FrameworkElement>().Tag())));
        Submitted(m_host->Core().SetEmulationMode(m_selected,static_cast<EmulationMode>(mode)));
        // Selection always reflects the runtime snapshot, including rejected requests.
        Update(*m_host->Core().GetSnapshot());
    }
}
void OverviewPage::OpenPage(Windows::Foundation::IInspectable const& sender,RoutedEventArgs const&) {
    if(m_navigate)m_navigate(unbox_value<hstring>(sender.as<FrameworkElement>().Tag()));
}
void OverviewPage::LayoutChanged(Windows::Foundation::IInspectable const&,SizeChangedEventArgs const& args) {
    const bool narrow=args.NewSize().Width<700;
    ControllerColumn().Width(GridLengthHelper::FromValueAndType(narrow?1:350,narrow?GridUnitType::Star:GridUnitType::Pixel));
    Grid::SetColumnSpan(ControllerPanel(),narrow?2:1);
    Grid::SetColumn(MainDashboard(),narrow?0:1);
    Grid::SetRow(MainDashboard(),narrow?1:0);
    Grid::SetColumnSpan(MainDashboard(),narrow?2:1);
}
}




