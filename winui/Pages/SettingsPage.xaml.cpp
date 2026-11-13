#include "pch.h"
#include "Services/Localization.h"
#include "SettingsPage.xaml.h"
#include "ResponsivePageLayout.h"
#include "SettingsPage.xaml.g.hpp"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace winrt::DualsenseZNative::implementation {
SettingsPage::SettingsPage() {
    InitializeComponent();NavigationCacheMode(Navigation::NavigationCacheMode::Required);
    ResponsivePageLayout::Attach(ComponentsLayout(),1,1,600);
    ComponentsLayout().SizeChanged([](auto const& sender,SizeChangedEventArgs const& args){sender.as<Grid>().RowSpacing(args.NewSize().Width<600?12:0);});
    ResponsivePageLayout::Attach(AboutLayout(),2,1,680);
    AboutLayout().SizeChanged([this](auto const&,SizeChangedEventArgs const& args){
        const bool narrow=args.NewSize().Width<680;
        AboutLayout().RowSpacing(narrow?16:0);
        AboutDivider().Visibility(narrow?Visibility::Collapsed:Visibility::Visible);
        VersionDetails().Margin(narrow?Thickness{0,0,0,0}:Thickness{20,0,0,0});
    });
    for(auto const& row:{LanguageRow(),DisconnectBluetoothRow(),TrayRow(),TrayStartupRow(),StartWithWindowsRow(),AssociateRow()}) {
        row.SizeChanged([](auto const& sender,SizeChangedEventArgs const& args){
            auto layout=sender.as<Grid>();auto control=layout.Children().GetAt(2).as<FrameworkElement>();
            const bool narrow=args.NewSize().Width<420;
            Grid::SetColumn(control,narrow?1:2);Grid::SetRow(control,narrow?1:0);
            layout.RowSpacing(narrow?8:0);
        });
    }
}
void SettingsPage::Update(ApplicationSnapshot const& s) {
    Language().IsEnabled(s.running);DisconnectBluetooth().IsEnabled(s.running);TrayMode().IsEnabled(s.running);TrayStartup().IsEnabled(s.running);StartWithWindows().IsEnabled(s.running);Associate().IsEnabled(s.running);
    ViGEmStatus().Text(NativeLocalization::Text(s.emulationAvailable?L"@WinUI.ComponentAvailable@":L"@WinUI.ComponentUnavailable@"));
    HidHideStatus().Text(NativeLocalization::Text(s.hidingAvailable?L"@WinUI.ComponentAvailable@":L"@WinUI.HidHideAndAdministratorModeRequired@"));
    if(m_pending)return;
    m_updating=true;m_draft=s.preferences;
    for(uint32_t i=0;i<Language().Items().Size();++i) {
        auto item=Language().Items().GetAt(i).as<ComboBoxItem>();
        if(to_string(unbox_value<hstring>(item.Tag()))==m_draft.language&&Language().SelectedIndex()!=int(i))Language().SelectedIndex(i);
    }
    if(DisconnectBluetooth().IsOn()!=m_draft.disconnectBluetoothOnExit)DisconnectBluetooth().IsOn(m_draft.disconnectBluetoothOnExit);
    // Drop-down order: never, when minimized, when closed, both.
    const int trayMode=(m_draft.hideToTrayOnMinimize?1:0)|(m_draft.hideToTrayOnClose?2:0);
    if(TrayMode().SelectedIndex()!=trayMode)TrayMode().SelectedIndex(trayMode);
    if(TrayStartup().IsOn()!=m_draft.hideToTrayOnStart)TrayStartup().IsOn(m_draft.hideToTrayOnStart);
    if(StartWithWindows().IsOn()!=m_draft.startWithWindows)StartWithWindows().IsOn(m_draft.startWithWindows);
    m_updating=false;
}
void SettingsPage::Submit() {
    if(NativeLocalization::applying||m_updating||!m_host)return;
    m_pending=m_host->Core().SetPreferences(m_draft);
    m_notifications.Submit(m_pending,L"@WinUI.PreferencesSaved@");
}
void SettingsPage::Completed(ApplicationCore::CommandResult const& r) {
    if(r.id!=m_pending)return;m_pending=0;

}
void SettingsPage::AssociateClicked(Windows::Foundation::IInspectable const&,RoutedEventArgs const&) {
    if(m_host)m_notifications.Submit(m_host->Core().AssociateProfiles());
}
void SettingsPage::LanguageChanged(Windows::Foundation::IInspectable const&,SelectionChangedEventArgs const&) {
    if(NativeLocalization::applying||m_updating||!Language().SelectedItem())return;
    m_draft.language=to_string(unbox_value<hstring>(Language().SelectedItem().as<ComboBoxItem>().Tag()));Submit();
}
void SettingsPage::TrayModeChanged(Windows::Foundation::IInspectable const&,SelectionChangedEventArgs const&) {
    const int mode=TrayMode().SelectedIndex();
    if(NativeLocalization::applying||m_updating||mode<0)return;
    m_draft.hideToTrayOnMinimize=(mode&1)!=0;m_draft.hideToTrayOnClose=(mode&2)!=0;Submit();
}
void SettingsPage::PreferencesChanged(Windows::Foundation::IInspectable const&,RoutedEventArgs const&) {
    if(NativeLocalization::applying||m_updating)return;
    m_draft.hideToTrayOnStart=TrayStartup().IsOn();m_draft.startWithWindows=StartWithWindows().IsOn();
    m_draft.disconnectBluetoothOnExit=DisconnectBluetooth().IsOn();Submit();
}
}
