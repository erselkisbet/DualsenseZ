#include "pch.h"
#include "Services/Localization.h"
#include "ControllerPage.xaml.h"
#include "ResponsivePageLayout.h"
#include "ControllerPageHeader.h"
#include "ControllerPage.xaml.g.hpp"
#if __has_include("ControllerPage.g.cpp")
#include "ControllerPage.g.cpp"
#endif
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <shobjidl_core.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace winrt::DualsenseZNative::implementation {
ControllerPage::ControllerPage() {
    InitializeComponent();NavigationCacheMode(Navigation::NavigationCacheMode::Required);
    ControllerPageHeader::Attach(PageHeader());
    ResponsivePageLayout::Attach(InputLayout(),1,1,700);
    HideLayout().SizeChanged([this](auto const&,auto const&){ArrangeVisibility();});
    auto gaps=[](Grid const& grid,double spacing) {
        grid.SizeChanged([weak=make_weak(grid),spacing](auto const&,SizeChangedEventArgs const& args) {
            if(auto layout=weak.get())layout.RowSpacing(args.NewSize().Width<700?spacing:0);
        });
    };
    gaps(InputLayout(),20);
}
void ControllerPage::ArrangeVisibility() {
    auto layout=HideLayout();auto controls=layout.Children().GetAt(0).as<FrameworkElement>();
    const bool requirement=HideInfo().Visibility()==Visibility::Visible,narrow=layout.ActualWidth()<700,split=requirement&&!narrow;
    layout.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromValueAndType(1,GridUnitType::Star));
    layout.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromValueAndType(split?1:0,split?GridUnitType::Star:GridUnitType::Pixel));
    Grid::SetColumnSpan(controls,split?1:2);Grid::SetColumn(HideInfo(),split?1:0);Grid::SetRow(HideInfo(),narrow?1:0);Grid::SetColumnSpan(HideInfo(),split?1:2);
    layout.RowSpacing(requirement&&narrow?20:0);
}
void ControllerPage::Update(ApplicationSnapshot const& s) {
    m_updating=true;m_selected=s.selectedController;const auto& c=s.controllers[m_selected];
    ControllerSelector().IsEnabled(s.running);
    if(ControllerSelector().SelectedIndex()!=m_selected)ControllerSelector().SelectedIndex(m_selected);
    auto text=[](TextBlock const& b,hstring const& v){auto translated=NativeLocalization::Text(v);if(b.Text()!=translated)b.Text(translated);};
    text(ShortcutsTitle(),L"@WinUI.KeyboardShortcuts@");text(ShortcutsDescription(),L"@WinUI.KeyboardShortcutsDescription@");
    Empty().IsOpen(!c.connected);
    Empty().Visibility(c.connected?Visibility::Collapsed:Visibility::Visible);
    const bool emulationEnabled=s.running&&c.connected&&s.emulationAvailable;
    Share().IsEnabled(emulationEnabled);
    LeftDeadzone().IsEnabled(emulationEnabled);RightDeadzone().IsEnabled(emulationEnabled);LeftDeadzoneSlider().IsEnabled(emulationEnabled);RightDeadzoneSlider().IsEnabled(emulationEnabled);
    Wasd().IsEnabled(s.running&&c.connected);PsKey().IsEnabled(s.running&&c.connected);
    DriverInfo().IsOpen(!s.emulationAvailable);
    DriverInfo().Visibility(s.emulationAvailable?Visibility::Collapsed:Visibility::Visible);
    ModInfo().IsOpen(s.udpActive);
    ModInfo().Visibility(s.udpActive?Visibility::Visible:Visibility::Collapsed);
    if(LeftDeadzone().Value()!=c.leftDeadzone)LeftDeadzone().Value(c.leftDeadzone);
    if(RightDeadzone().Value()!=c.rightDeadzone)RightDeadzone().Value(c.rightDeadzone);
    if(LeftDeadzoneSlider().Value()!=c.leftDeadzone)LeftDeadzoneSlider().Value(c.leftDeadzone);
    if(RightDeadzoneSlider().Value()!=c.rightDeadzone)RightDeadzoneSlider().Value(c.rightDeadzone);
    if(Share().IsOn()!=c.shareAsSelect)Share().IsOn(c.shareAsSelect);
    if(Wasd().IsOn()!=c.analogWasd)Wasd().IsOn(c.analogWasd);
    if(PsKey().IsOn()!=c.psAsWindows)PsKey().IsOn(c.psAsWindows);
    if(Hidden().IsOn()!=c.hidden)Hidden().IsOn(c.hidden);
    Hidden().IsEnabled(s.running&&c.connected&&s.emulationAvailable&&s.hidingAvailable);
    HideInfo().IsOpen(!s.hidingAvailable);
    HideInfo().Visibility(s.hidingAvailable?Visibility::Collapsed:Visibility::Visible);
    if(auto label=NativeLocalization::Text(L"@WinUI.OpenSettings@");!NativeLocalization::IsString(OpenSettingsAction().Content(),label))OpenSettingsAction().Content(box_value(label));
    ArrangeVisibility();
    text(LeftState(),c.connected?to_hstring("@WinUI.X@"+std::to_string(c.stickAxes[0])+"@WinUI.Y@"+std::to_string(c.stickAxes[1])+(c.leftStickPressed?"@WinUI.L3Pressed@":"@WinUI.L3Released@")):L"@WinUI.LeftStickUnavailable@");
    text(RightState(),c.connected?to_hstring("@WinUI.X@"+std::to_string(c.stickAxes[2])+"@WinUI.Y@"+std::to_string(c.stickAxes[3])+(c.rightStickPressed?"@WinUI.R3Pressed@":"@WinUI.R3Released@")):L"@WinUI.RightStickUnavailable@");
    LeftFeedback().Opacity(c.connected?1.0:0.5);RightFeedback().Opacity(c.connected?1.0:0.5);
    LeftX().Value(c.stickAxes[0]);LeftY().Value(c.stickAxes[1]);RightX().Value(c.stickAxes[2]);RightY().Value(c.stickAxes[3]);
    auto dot=[connected=c.connected](Shapes::Ellipse const& marker,int x,int y){
        const auto position=[](int axis){return 64+54*std::clamp((axis-128)/127.0,-1.0,1.0);};
        Canvas::SetLeft(marker,connected?position(x):64);Canvas::SetTop(marker,connected?position(y):64);
    };
    dot(LeftDot(),c.stickAxes[0],c.stickAxes[1]);dot(RightDot(),c.stickAxes[2],c.stickAxes[3]);
    m_updating=false;
}
void ControllerPage::SelectController(Windows::Foundation::IInspectable const&,SelectionChangedEventArgs const&) {
    if(!m_updating&&!NativeLocalization::applying&&m_host&&ControllerSelector().SelectedIndex()>=0)
        m_notifications.Submit(m_host->Core().SelectController(ControllerSelector().SelectedIndex()),L"");
}
void ControllerPage::Submitted(ApplicationCore::RequestId id) {
    m_notifications.Submit(id);
}
void ControllerPage::OptionChanged(Windows::Foundation::IInspectable const& sender,RoutedEventArgs const&) {
    if(NativeLocalization::applying||m_updating||!m_host)return;
    auto toggle=sender.as<ToggleSwitch>();
    if(toggle==Hidden()){Submitted(m_host->Core().SetControllerHidden(m_selected,toggle.IsOn()));return;}
    auto option=toggle==Share()?ControllerOption::ShareAsSelect:toggle==Wasd()?ControllerOption::AnalogWasd:ControllerOption::PsAsWindows;
    Submitted(m_host->Core().SetControllerOption(m_selected,option,toggle.IsOn()?1:0));
}
void ControllerPage::DeadzoneChanged(NumberBox const& sender,NumberBoxValueChangedEventArgs const&) {
    if(NativeLocalization::applying||m_updating||!m_host||!std::isfinite(sender.Value()))return;
    Submitted(m_host->Core().SetControllerOption(m_selected,sender==LeftDeadzone()?ControllerOption::LeftDeadzone:ControllerOption::RightDeadzone,int(sender.Value())));
}
void ControllerPage::DeadzoneSliderChanged(Windows::Foundation::IInspectable const& sender,Primitives::RangeBaseValueChangedEventArgs const& args) {
    if(NativeLocalization::applying||m_updating||!m_host)return;
    Submitted(m_host->Core().SetControllerOption(m_selected,sender.as<Slider>()==LeftDeadzoneSlider()?ControllerOption::LeftDeadzone:ControllerOption::RightDeadzone,int(std::round(args.NewValue()))));
}
void ControllerPage::OpenSettingsClicked(Windows::Foundation::IInspectable const&,RoutedEventArgs const&) {
    if(m_openSettings)m_openSettings();
}
}
