#include "pch.h"
#include "FeaturePage.xaml.h"
#include "ResponsivePageLayout.h"
#include "ControllerPageHeader.h"
#include "FeaturePage.xaml.g.hpp"
#if __has_include("FeaturePage.g.cpp")
#include "FeaturePage.g.cpp"
#endif
#include <cmath>
#include <winrt/Windows.Globalization.NumberFormatting.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Json.h>
#include <shobjidl_core.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "Services/Localization.h"
#include "core/batteryLightbar.hpp"
#include "core/lightingLevels.hpp"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace {
struct LightingColorState {
    std::array<float,3> last{-1,-1,-1};
};
std::string ColorHex(std::array<float,3> const& color) {
    std::ostringstream out;out<<'#'<<std::uppercase<<std::hex<<std::setfill('0');
    for(float channel:color)out<<std::setw(2)<<int(std::clamp(channel,0.f,1.f)*255+0.5f);
    return out.str();
}
TextBlock Text(std::string const& text,double size=14) {
    NativeLocalization::Invalidate();
    TextBlock b;b.Text(to_hstring(text));b.TextWrapping(TextWrapping::Wrap);b.FontSize(size);return b;
}
ComboBox Choice(std::string const& label,std::vector<std::string> const& items) {
    NativeLocalization::Invalidate();
    ComboBox c;c.Header(box_value(to_hstring(label)));c.HorizontalAlignment(HorizontalAlignment::Stretch);
    for(auto& item:items){ComboBoxItem entry;entry.Content(box_value(to_hstring(item)));c.Items().Append(entry);}c.SelectedIndex(0);return c;
}
NumberBox Number(std::string const& name,double min,double max,double step=1) {
    NativeLocalization::Invalidate();
    NumberBox n;n.Header(box_value(to_hstring(name)));n.Minimum(min);n.Maximum(max);n.SmallChange(step);
    n.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Inline);
    Windows::Globalization::NumberFormatting::DecimalFormatter formatter;formatter.FractionDigits(step<1?2:0);formatter.IntegerDigits(1);n.NumberFormatter(formatter);return n;
}
// Evenly sized, responsive preset tiles and telemetry groups.
void AdaptiveColumns(Grid const& grid,int wideColumns,double wideWidth,double mediumWidth) {
    auto arrange=[weak=make_weak(grid),wideColumns,wideWidth,mediumWidth](double width){
        if(auto layout=weak.get()) {
            const int count=width>=wideWidth?wideColumns:width>=mediumWidth?2:1;
            if(layout.ColumnDefinitions().Size()!=uint32_t(count)){layout.ColumnDefinitions().Clear();for(int i=0;i<count;++i)layout.ColumnDefinitions().Append(ColumnDefinition());}
            const uint32_t rows=(layout.Children().Size()+count-1)/count;
            if(layout.RowDefinitions().Size()!=rows){layout.RowDefinitions().Clear();for(uint32_t i=0;i<rows;++i){RowDefinition row;row.Height(GridLengthHelper::Auto());layout.RowDefinitions().Append(row);}}
            for(uint32_t i=0;i<layout.Children().Size();++i){auto item=layout.Children().GetAt(i).as<FrameworkElement>();Grid::SetColumn(item,i%count);Grid::SetRow(item,i/count);}
        }
    };
    grid.SizeChanged([arrange](auto const&,SizeChangedEventArgs const& args){arrange(args.NewSize().Width);});arrange(grid.ActualWidth());
}
// Adaptive Triggers uses dividers only while its internal columns are side by side.
void TriggerDivider(Grid const& grid,double breakpoint) {
    Border divider;divider.Width(1);divider.Margin(Thickness{0,8,0,8});divider.HorizontalAlignment(HorizontalAlignment::Center);
    divider.Background(Application::Current().Resources().Lookup(box_value(L"CardStrokeColorDefaultBrush")).as<Media::Brush>());
    Grid::SetColumnSpan(divider,2);Grid::SetRowSpan(divider,2);grid.Children().Append(divider);
    grid.SizeChanged([divider,breakpoint](auto const&,SizeChangedEventArgs const& args){divider.Visibility(args.NewSize().Width<breakpoint?Visibility::Collapsed:Visibility::Visible);});
}
void Enable(Controls::Panel const& panel,bool enabled) {
    for(auto child:panel.Children()) {
        if(auto control=child.try_as<Control>()) {if(control.IsEnabled()!=enabled)control.IsEnabled(enabled);}
        else if(auto nested=child.try_as<Controls::Panel>())Enable(nested,enabled);
    }
}
// Compact switch without an On/Off caption; its row label names it.
ToggleSwitch RowToggle() {
    ToggleSwitch toggle;toggle.MinWidth(44);toggle.Width(44);
    auto hidden=Markup::XamlReader::Load(L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'><Grid Visibility='Collapsed'/></DataTemplate>").as<DataTemplate>();
    toggle.OnContentTemplate(hidden);toggle.OffContentTemplate(hidden);return toggle;
}
// One settings row, shared by every page: label (and optional description) on the
// left, control on the right; the control drops below the label when the row is narrow.
void SettingRow(StackPanel const& parent,std::string const& label,FrameworkElement const& control,std::string const& description={},double size=14) {
    Grid row;row.ColumnSpacing(12);row.RowSpacing(8);
    for(int i=0;i<2;++i){ColumnDefinition column;if(i==1)column.Width(GridLengthHelper::Auto());row.ColumnDefinitions().Append(column);RowDefinition definition;definition.Height(GridLengthHelper::Auto());row.RowDefinitions().Append(definition);}
    StackPanel labels;labels.Spacing(2);labels.VerticalAlignment(VerticalAlignment::Center);labels.Children().Append(Text(label,size));
    if(!description.empty()){auto note=Text(description,12);note.Opacity(0.7);labels.Children().Append(note);}
    row.Children().Append(labels);
    control.HorizontalAlignment(HorizontalAlignment::Right);control.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(control,1);row.Children().Append(control);parent.Children().Append(row);
    Automation::AutomationProperties::SetName(control,to_hstring(label));
    row.SizeChanged([control](auto const&,SizeChangedEventArgs const& args){const bool narrow=args.NewSize().Width<320;Grid::SetColumn(control,narrow?0:1);Grid::SetRow(control,narrow?1:0);});
}
// Slider with its current value beside it, for bounded numeric settings.
struct RangeControl {Grid root;Slider slider;TextBlock value;};
RangeControl Range(double minimum,double maximum,double step) {
    RangeControl r;r.root.ColumnSpacing(8);
    for(int i=0;i<2;++i){ColumnDefinition column;column.Width(GridLengthHelper::Auto());r.root.ColumnDefinitions().Append(column);}
    r.slider.Minimum(minimum);r.slider.Maximum(maximum);r.slider.StepFrequency(step);r.slider.SmallChange(step);r.slider.Width(130);r.slider.VerticalAlignment(VerticalAlignment::Center);r.slider.IsThumbToolTipEnabled(false);
    r.value.Width(36);r.value.TextAlignment(TextAlignment::Right);r.value.VerticalAlignment(VerticalAlignment::Center);
    r.root.Children().Append(r.slider);Grid::SetColumn(r.value,1);r.root.Children().Append(r.value);return r;
}
hstring RangeText(double value,double step) {
    std::ostringstream out;out<<std::fixed<<std::setprecision(step<1?2:0)<<value;return to_hstring(out.str());
}
// Settings-style card header: icon tile, title with a status line under it, switch on the right.
struct CardHeader {TextBlock title{nullptr},note{nullptr};ToggleSwitch toggle{nullptr};};
CardHeader HeaderRow(StackPanel const& card,wchar_t const* glyph,std::string const& title,std::string const& description={},FrameworkElement const& control=nullptr) {
    CardHeader header;Grid row;row.ColumnSpacing(12);
    for(int i=0;i<3;++i){ColumnDefinition column;if(i!=1)column.Width(GridLengthHelper::Auto());row.ColumnDefinitions().Append(column);}
    Border tile;tile.Width(36);tile.Height(36);tile.CornerRadius(CornerRadius{8,8,8,8});tile.VerticalAlignment(VerticalAlignment::Top);
    tile.Background(Application::Current().Resources().Lookup(box_value(L"ControlFillColorDefaultBrush")).as<Media::Brush>());
    FontIcon icon;icon.Glyph(glyph);icon.FontSize(18);tile.Child(icon);row.Children().Append(tile);
    StackPanel labels;labels.Spacing(3);labels.VerticalAlignment(VerticalAlignment::Center);
    header.title=Text(title);header.title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());labels.Children().Append(header.title);
    header.note=Text(description,12);header.note.Opacity(0.7);labels.Children().Append(header.note);
    Grid::SetColumn(labels,1);row.Children().Append(labels);
    // A caller-supplied control takes the switch's place on the right.
    if(control){control.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(control,2);row.Children().Append(control);Automation::AutomationProperties::SetName(control,to_hstring(title));card.Children().Append(row);return header;}
    header.toggle=RowToggle();header.toggle.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(header.toggle,2);row.Children().Append(header.toggle);
    Automation::AutomationProperties::SetName(header.toggle,to_hstring(title));
    card.Children().Append(row);return header;
}
void Divider(StackPanel const& card) {
    Border line;line.Height(1);line.Background(Application::Current().Resources().Lookup(box_value(L"CardStrokeColorDefaultBrush")).as<Media::Brush>());card.Children().Append(line);
}
// Turns a column of cards into a grid whose last card takes the spare height, so side-by-side columns end level.
Grid FillColumn(StackPanel const& stack) {
    Grid column;column.RowSpacing(stack.Spacing());
    std::vector<UIElement> cards;for(auto const& child:stack.Children())cards.push_back(child);stack.Children().Clear();
    for(size_t i=0;i<cards.size();++i) {
        RowDefinition row;row.Height(i+1==cards.size()?GridLengthHelper::FromValueAndType(1,GridUnitType::Star):GridLengthHelper::Auto());column.RowDefinitions().Append(row);
        Grid::SetRow(cards[i].as<FrameworkElement>(),int(i));column.Children().Append(cards[i]);
    }
    return column;
}
void Link(StackPanel const& panel,std::string const& label,std::string const& url) {
    HyperlinkButton link;link.Content(box_value(to_hstring(label)));link.NavigateUri(Windows::Foundation::Uri(to_hstring(url)));panel.Children().Append(link);
}
}
namespace winrt::DualsenseZNative::implementation {
FeaturePage::FeaturePage() {InitializeComponent();NavigationCacheMode(Navigation::NavigationCacheMode::Required);}
FeaturePage::Panel FeaturePage::Card(Controls::Panel const& parent,std::string const& title) {
    Border b;b.Style(Application::Current().Resources().Lookup(box_value(L"Card")).as<Microsoft::UI::Xaml::Style>());
    b.CornerRadius(Microsoft::UI::Xaml::CornerRadius{12,12,12,12});b.Padding(Microsoft::UI::Xaml::Thickness{20,20,20,20});
    Panel panel;panel.Spacing(16);if(!title.empty())panel.Children().Append(Text(title,20));b.Child(panel);parent.Children().Append(b);return panel;
}
Controls::Button FeaturePage::Action(std::string const& label,std::function<void(FeaturePage&)> action) {
    NativeLocalization::Invalidate();
    Controls::Button b;b.Content(box_value(to_hstring(label)));
    b.Click([weak=get_weak(),action=std::move(action)](auto&&,auto&&){if(auto self=weak.get())action(*self);});return b;
}
void FeaturePage::Button(Panel const& panel,std::string const& label,std::function<void(FeaturePage&)> action) {
    panel.Children().Append(Action(label,std::move(action)));
}
void FeaturePage::Submit(ApplicationCore::RequestId id) {
    m_notifications.Submit(id);
}
void FeaturePage::OnNavigatedTo(Navigation::NavigationEventArgs const& args) {
    m_name=to_string(unbox_value<hstring>(args.Parameter()));m_capture=-1;
    auto found=m_views.find(m_name);
    m_view=found==m_views.end()?m_views.emplace(m_name,Build(m_name)).first->second:found->second;
    Surface().Children().Clear();Surface().Children().Append(m_view->root);
    NativeLocalization::Invalidate();
}
std::shared_ptr<FeaturePage::View> FeaturePage::Build(std::string const& name) {
    auto v=std::make_shared<View>();v->content.Spacing(16);v->content.HorizontalAlignment(HorizontalAlignment::Stretch);v->content.Margin(Thickness{0,0,20,0});
    v->root.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);v->root.Content(v->content);
    const char* title=name=="Profiles"?"@WinUI.Profiles@":name=="Adaptive Triggers"?"@WinUI.AdaptiveTriggers@":name=="Sound & Haptics"?"@WinUI.SoundHaptics@":name=="Lighting"?"@Lighting@":name=="Motion & Touch"?"@WinUI.MotionTouch@":name=="Diagnostics"?"@Diagnostics@":"@Diagnostics@";
    Panel labels;labels.Spacing(6);labels.Children().Append(Text(title,28));
    const char* description=name=="Profiles"?"@WinUI.ProfilesDescription@":name=="Adaptive Triggers"?"@TriggersDescription@":name=="Sound & Haptics"?"@WinUI.AudioAndHapticFeedback@":name=="Lighting"?"@LightingDescription@":name=="Motion & Touch"?"@MotionDescription@":name=="Diagnostics"?"@WinUI.LiveInputAndHaptics@":"@WinUI.LiveInputAndHaptics@";
    auto subtitle=Text(description);subtitle.Opacity(0.7);labels.Children().Append(subtitle);
    InfoBar state;state.IsClosable(false);state.Visibility(Visibility::Collapsed);v->content.Children().Append(state);
    v->bindings.push_back([state,name](auto const& s,bool){auto& c=s.controllers[s.selectedController];
        const bool meaningful=!s.running||!c.connected||s.udpActive;
        state.IsOpen(meaningful);state.Visibility(meaningful?Visibility::Visible:Visibility::Collapsed);
        if(!meaningful)return;
        NativeLocalization::SetInfo(state,!s.running?L"@WinUI.RuntimeUnavailable@":!c.connected?L"@WinUI.NoControllerConnected@":L"@WinUI.AGameOrModIsControllingOutput2@",
            !s.running?L"@WinUI.TheRuntimeIsNotReadyOrIsBusyPleaseTryAgain@":s.udpActive?L"@WinUI.AUDPModIsControllingLightingAndAdaptiveTriggers@":name=="Diagnostics"?L"@WinUI.ReadOnlyControllerTelemetry@":L"@WinUI.ConnectAControllerToApplyTheseSettingsToADevice@");});
    {
        auto selector=Choice("@WinUI.SelectedController@",{"@WinUI.Controller1@","@WinUI.Controller2@","@WinUI.Controller3@","@WinUI.Controller4@"});v->selector=selector;
        selector.SelectionChanged([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host){auto index=sender.template as<ComboBox>().SelectedIndex();if(index>=0)self->m_notifications.Submit(self->m_host->Core().SelectController(index),L"");}});
        v->bindings.push_back([selector](auto const& s,bool){selector.IsEnabled(s.running);if(selector.SelectedIndex()!=s.selectedController)selector.SelectedIndex(s.selectedController);});
    }
    Grid header;header.Children().Append(labels);header.Children().Append(v->selector);
    ControllerPageHeader::Attach(header);v->content.Children().InsertAt(0,header);
    v->dashboard.ColumnSpacing(16);v->dashboard.RowSpacing(16);
    v->dashboard.ColumnDefinitions().Append(ColumnDefinition());v->dashboard.ColumnDefinitions().Append(ColumnDefinition());
    v->dashboard.RowDefinitions().Append(RowDefinition());v->dashboard.RowDefinitions().Append(RowDefinition());
    v->left.Spacing(16);v->right.Spacing(16);
    v->dashboard.Children().Append(v->left);Grid::SetColumn(v->right,1);v->dashboard.Children().Append(v->right);
    v->content.Children().Append(v->dashboard);
    ResponsivePageLayout::Attach(v->dashboard);
    // Row spacing only separates the stacked columns; side by side the second row is empty.
    v->dashboard.RowSpacing(0);v->dashboard.SizeChanged([](auto const& sender,SizeChangedEventArgs const& args){sender.template as<Grid>().RowSpacing(args.NewSize().Width<700?16:0);});
    if(name=="Adaptive Triggers") {
        v->content.Spacing(24);
        Triggers(v);TriggerBehavior(v);TriggerPresets(v);
        auto effects=v->left.Children().GetAt(0),behavior=v->left.Children().GetAt(1);
        auto presets=v->right.Children().GetAt(0);
        v->left.Children().Clear();v->right.Children().Clear();
        v->content.Children().RemoveAt(v->content.Children().Size()-1);
        v->content.Children().InsertAt(2,presets);v->content.Children().Append(effects);v->content.Children().Append(behavior);
    }
    else if(name=="Sound & Haptics") {
        auto p=Card(v->left,"");auto t=RowToggle();SettingRow(p,"@Audio passthrough@",t,{},20);
        t.Toggled([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)self->Submit(self->m_host->Core().SetAudioPassthroughEnabled(self->m_selected,sender.template as<ToggleSwitch>().IsOn()));});
        auto status=Text("");p.Children().Append(status);
        v->bindings.push_back([t,status,last=-1](auto const& s,bool force)mutable{auto& c=s.controllers[s.selectedController];t.IsEnabled(s.running&&c.connected);if(force||last!=int(c.audioPassthroughEnabled))t.IsOn(c.audioPassthroughEnabled);last=c.audioPassthroughEnabled;NativeLocalization::SetText(status,hstring(c.audioFailed?L"@WinUI.AudioCaptureCouldNotBeStarted@":L"@WinUI.WindowsUsesTheDefaultAudioOutputAsTheCaptureSource@"));});
        Options(v,name);Telemetry(v);
    } else if(name=="Profiles") { Profiles(v);
    } else if(name=="Lighting") { LightingOptions(v);
    } else if(name=="Motion & Touch") {
        Options(v,name);auto p=Card(v->right,"@WinUI.ActivationButtons@");
        p.Children().Append(Text("@WinUI.CaptureWaitsThreeSecondsHoldTheDesiredControllerButtonsUntilCaptureCompletesTheMicrophoneButtonAlsoTogglesTheGyroGate@"));
        for(int k=0;k<3;++k){
            std::string title=k==0?"@WinUI.GyroStick@":k==1?"@WinUI.GyroMouse@":"@WinUI.LeftMouseClick@";
            // One row per shortcut: its name with the assigned buttons underneath, actions on the right.
            Grid row;row.ColumnSpacing(12);row.ColumnDefinitions().Append(ColumnDefinition());ColumnDefinition actionsColumn;actionsColumn.Width(GridLengthHelper::Auto());row.ColumnDefinitions().Append(actionsColumn);
            Panel labels;labels.Spacing(2);labels.VerticalAlignment(VerticalAlignment::Center);labels.Children().Append(Text(title));
            auto assigned=Text("",12);assigned.Opacity(0.7);labels.Children().Append(assigned);row.Children().Append(labels);
            Panel actions;actions.Orientation(Orientation::Horizontal);actions.Spacing(8);actions.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(actions,1);row.Children().Append(actions);p.Children().Append(row);
            v->bindings.push_back([assigned,k](auto const& s,bool){auto value=s.controllers[s.selectedController].hotkeyNames[k];NativeLocalization::SetText(assigned,value.empty()?std::string("@None@"):value);});
            auto capture=Action("@WinUI.CaptureButton@",[k](auto& self){self.m_capture=k;self.m_captureSlot=self.m_selected;self.m_captureEnd=std::chrono::steady_clock::now()+std::chrono::seconds(3);self.m_notifications.Show(L"@WinUI.HoldTheControllerButtonsForThreeSeconds@",InfoBarSeverity::Informational);});
            auto clear=Action("@WinUI.ClearButton@",[k](auto& self){if(self.m_host)self.Submit(self.m_host->Core().SetHotkey(self.m_selected,HotkeyTarget(k),0));});
            Automation::AutomationProperties::SetName(capture,to_hstring("@WinUI.Capture@"+title));Automation::AutomationProperties::SetName(clear,to_hstring("@WinUI.Clear@"+title));
            actions.Children().Append(capture);actions.Children().Append(clear);}
        Telemetry(v);
    } else if(name=="Diagnostics") {
        Telemetry(v);DeadzoneTelemetry(v);MotionTelemetry(v);UdpStatus(v);
        // Live input and the deadzone preview share a row; motion values and mod status span the page below it.
        for(int k=0;k<2;++k){auto card=v->right.Children().GetAt(1);v->right.Children().RemoveAt(1);v->content.Children().Append(card);}
    }
    if(name=="Sound & Haptics"||name=="Motion & Touch"||name=="Diagnostics") {
        auto left=FillColumn(v->left),right=FillColumn(v->right);
        v->dashboard.Children().Clear();v->dashboard.Children().Append(left);Grid::SetColumn(right,1);v->dashboard.Children().Append(right);
    }
    return v;
}
void FeaturePage::LightingOptions(std::shared_ptr<View> const& v) {
    // Lighting lays out its own rows instead of the two-column dashboard; cards sharing a row share its height.
    v->content.Children().RemoveAt(v->content.Children().Size()-1);
    auto section=[&v](){Grid row;row.ColumnSpacing(16);row.RowSpacing(16);v->content.Children().Append(row);return row;};
    auto note=[](Panel const& panel,char const* text){auto block=Text(text,12);block.Opacity(0.7);panel.Children().Append(block);};

    auto top=section();
    auto activeCard=Card(top,"@WinUI.ActiveLighting@");activeCard.Spacing(6);
    auto activeSurface=top.Children().GetAt(0).as<Border>();
    activeSurface.BorderBrush(Application::Current().Resources().Lookup(box_value(L"AccentFillColorDefaultBrush")).as<Media::Brush>());
    activeSurface.BorderThickness(Thickness{1,1,1,1});
    activeSurface.Background(Application::Current().Resources().Lookup(box_value(L"CardBackgroundFillColorSecondaryBrush")).as<Media::Brush>());
    auto activeName=Text("",18);activeName.FontWeight(Windows::UI::Text::FontWeights::SemiBold());activeCard.Children().Append(activeName);
    auto activeDetails=Text("",12);activeDetails.Opacity(0.7);activeCard.Children().Append(activeDetails);
    auto global=Card(top,"@WinUI.GlobalLightbarBrightness@");global.Spacing(12);
    Grid brightnessRow;brightnessRow.ColumnDefinitions().Append(ColumnDefinition());
    ColumnDefinition percentWidth;percentWidth.Width(GridLengthHelper::FromValueAndType(52,GridUnitType::Pixel));brightnessRow.ColumnDefinitions().Append(percentWidth);
    Slider brightness;brightness.Minimum(0);brightness.Maximum(100);brightness.StepFrequency(1);brightnessRow.Children().Append(brightness);
    Automation::AutomationProperties::SetName(brightness,L"@WinUI.GlobalLightbarBrightness@");
    auto percent=Text("");percent.VerticalAlignment(VerticalAlignment::Center);percent.Margin(Thickness{8,0,0,0});
    Grid::SetColumn(percent,1);brightnessRow.Children().Append(percent);global.Children().Append(brightnessRow);
    brightness.ValueChanged([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
        self->Submit(self->m_host->Core().SetLightbarBrightness(self->m_selected,float(sender.template as<Slider>().Value()/100)));});
    AdaptiveColumns(top,2,700,700);

    // The three base modes: battery and cycle side by side, static color across the page.
    const std::array<BaseLightbarMode,3> modes{BaseLightbarMode::Battery,BaseLightbarMode::ColorCycle,BaseLightbarMode::Static};
    const std::array<std::string,3> titles{"@WinUI.BatteryBasedColor@","@WinUI.AutomaticColorCycle@","@WinUI.StaticColor@"};
    const wchar_t* glyphs[]{L"",L"",L""};
    auto modeRow=section();
    std::array<Panel,3> settings{Card(modeRow,""),Card(modeRow,""),Card(v->content,"")};
    AdaptiveColumns(modeRow,2,700,700);
    std::array<CardHeader,3> headers;
    for(int i=0;i<3;++i){settings[i].Spacing(12);headers[i]=HeaderRow(settings[i],glyphs[i],titles[i]);Divider(settings[i]);}
    auto battery=headers[0].toggle,cycle=headers[1].toggle,staticToggle=headers[2].toggle;

    settings[0].Children().Append(Text("@WinUI.BatteryLightbarLegend@"));
    note(settings[0],"@WinUI.BatteryPriorityExplanation@");
    battery.Toggled([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
        self->Submit(self->m_host->Core().SetBatteryBasedLightbarEnabled(self->m_selected,sender.template as<ToggleSwitch>().IsOn()));});

    cycle.Toggled([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
        self->Submit(self->m_host->Core().SetBaseLightbarMode(self->m_selected,sender.template as<ToggleSwitch>().IsOn()?BaseLightbarMode::ColorCycle:BaseLightbarMode::None));});
    auto speed=Choice("@WinUI.ColorCycleSpeed@",{"@WinUI.VerySlow@","@WinUI.Slow@","@WinUI.Normal@","@WinUI.Fast@","@WinUI.VeryFast@"});speed.Header(nullptr);speed.Width(180);SettingRow(settings[1],"@WinUI.ColorCycleSpeed@",speed);
    note(settings[1],"@WinUI.CyclePriorityExplanation@");
    speed.SelectionChanged([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host){
        int index=sender.template as<ComboBox>().SelectedIndex();if(index>=0&&index<int(LightbarCycleSpeeds.size()))self->Submit(self->m_host->Core().SetFeature(self->m_selected,FeatureOption::DiscoSpeed,LightbarCycleSpeeds[index]));}});

    staticToggle.Toggled([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
        self->Submit(self->m_host->Core().SetBaseLightbarMode(self->m_selected,sender.template as<ToggleSwitch>().IsOn()?BaseLightbarMode::Static:BaseLightbarMode::None));});
    // Static color: presets for one click, the picker with hex and RGB fields in a flyout.
    auto colorState=std::make_shared<LightingColorState>();
    auto submitColor=[weak=get_weak(),colorState](std::array<float,3> const& rgb){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host&&ColorHex(rgb)!=ColorHex(colorState->last))
        self->Submit(self->m_host->Core().SetLightbarColor(self->m_selected,rgb));};
    // Compact row: preset swatches on the left, a drop-down holding the full picker on the right.
    Grid colorRow;colorRow.ColumnSpacing(12);colorRow.RowSpacing(12);settings[2].Children().Append(colorRow);
    Panel presetRow;presetRow.Orientation(Orientation::Horizontal);presetRow.Spacing(6);presetRow.VerticalAlignment(VerticalAlignment::Center);colorRow.Children().Append(presetRow);
    const std::array<std::array<uint8_t,3>,9> presetColors{{{0,55,255},{0,200,255},{0,255,64},{255,208,0},{255,106,0},{255,0,0},{255,0,160},{128,0,255},{255,255,255}}};
    auto presets=std::make_shared<std::vector<Controls::Button>>();
    for(auto const& rgb:presetColors) {
        const std::array<float,3> value{rgb[0]/255.f,rgb[1]/255.f,rgb[2]/255.f};
        Controls::Button preset;preset.Width(32);preset.Height(32);preset.MinWidth(0);preset.MinHeight(0);preset.Padding(Thickness{0,0,0,0});
        Border fill;fill.Width(20);fill.Height(20);fill.CornerRadius(Microsoft::UI::Xaml::CornerRadius{4,4,4,4});fill.Background(Media::SolidColorBrush(Windows::UI::Color{255,rgb[0],rgb[1],rgb[2]}));preset.Content(fill);
        Automation::AutomationProperties::SetName(preset,to_hstring(ColorHex(value)));ToolTipService::SetToolTip(preset,box_value(to_hstring(ColorHex(value))));
        preset.Click([submitColor,value](auto&&,auto&&){submitColor(value);});
        presetRow.Children().Append(preset);presets->push_back(preset);
    }
    Media::SolidColorBrush swatchBrush;Border swatch;swatch.Width(18);swatch.Height(18);swatch.CornerRadius(Microsoft::UI::Xaml::CornerRadius{4,4,4,4});swatch.Background(swatchBrush);
    swatch.BorderThickness(Thickness{1,1,1,1});swatch.BorderBrush(Application::Current().Resources().Lookup(box_value(L"CardStrokeColorDefaultBrush")).as<Media::Brush>());
    auto pickLabel=Text("@WinUI.CustomColor@");pickLabel.TextWrapping(TextWrapping::NoWrap);pickLabel.VerticalAlignment(VerticalAlignment::Center);
    Panel pickContent;pickContent.Orientation(Orientation::Horizontal);pickContent.Spacing(8);pickContent.Children().Append(swatch);pickContent.Children().Append(pickLabel);
    DropDownButton pick;pick.Content(pickContent);pick.HorizontalAlignment(HorizontalAlignment::Right);pick.VerticalAlignment(VerticalAlignment::Center);colorRow.Children().Append(pick);
    Automation::AutomationProperties::SetName(pick,L"@WinUI.CustomColor@");
    Panel picker;picker.Spacing(12);picker.Width(312);
    ColorPicker color;color.IsAlphaEnabled(false);color.IsHexInputVisible(false);color.IsColorChannelTextInputVisible(false);color.IsColorPreviewVisible(false);
    picker.Children().Append(color);
    color.ColorChanged([weak=get_weak()](auto const&,ColorChangedEventArgs const& e){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host){
        // Preserve native HSV value selection, including an intentional black.
        const auto selected=e.NewColor();
        self->Submit(self->m_host->Core().SetLightbarColor(self->m_selected,{selected.R/255.f,selected.G/255.f,selected.B/255.f}));}});
    Grid fields;fields.ColumnSpacing(8);for(int i=0;i<4;++i){ColumnDefinition column;column.Width(GridLengthHelper::FromValueAndType(i==0?1.5:1,GridUnitType::Star));fields.ColumnDefinitions().Append(column);}
    picker.Children().Append(fields);
    TextBox hex;hex.Header(box_value(L"Hex"));hex.MaxLength(7);hex.CharacterCasing(CharacterCasing::Upper);fields.Children().Append(hex);
    auto applyHex=[submitColor,hex]{
        auto text=to_string(hex.Text());text.erase(std::remove_if(text.begin(),text.end(),[](unsigned char c){return c=='#'||std::isspace(c);}),text.end());
        if(text.size()!=6||!std::all_of(text.begin(),text.end(),[](unsigned char c){return std::isxdigit(c)!=0;}))return;
        const unsigned long packed=std::stoul(text,nullptr,16);
        submitColor({((packed>>16)&255)/255.f,((packed>>8)&255)/255.f,(packed&255)/255.f});};
    hex.LostFocus([applyHex](auto&&,auto&&){applyHex();});
    hex.KeyDown([applyHex](auto&&,Input::KeyRoutedEventArgs const& e){if(e.Key()==Windows::System::VirtualKey::Enter)applyHex();});
    std::array<NumberBox,3> channels;// Plain letters: the flyout lives outside the page tree, so resource tokens there are never translated.
    const char* channelNames[]{"R","G","B"};
    for(int i=0;i<3;++i){channels[i]=Number(channelNames[i],0,255);channels[i].SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Hidden);Grid::SetColumn(channels[i],i+1);fields.Children().Append(channels[i]);}
    for(int i=0;i<3;++i)channels[i].ValueChanged([submitColor,channels](auto&&,auto&&){
        std::array<float,3> rgb{};for(int k=0;k<3;++k){const double value=channels[k].Value();if(!std::isfinite(value))return;rgb[k]=float(std::clamp(value,0.0,255.0)/255);}
        submitColor(rgb);});
    Flyout pickerFlyout;pickerFlyout.Content(picker);pickerFlyout.Placement(Primitives::FlyoutPlacementMode::BottomEdgeAlignedRight);pick.Flyout(pickerFlyout);
    AdaptiveColumns(colorRow,2,520,520);

    // The remaining switches share one full-width card, one row each, like the Settings page.
    auto extras=Card(v->content,"");extras.Spacing(12);
    auto featureToggle=[this](Panel const& panel,wchar_t const* glyph,char const* title,char const* description,FeatureOption option){
        auto header=HeaderRow(panel,glyph,title,description);
        header.toggle.Toggled([weak=get_weak(),option](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
            self->Submit(self->m_host->Core().SetFeature(self->m_selected,option,sender.template as<ToggleSwitch>().IsOn()?1:0));});
        return header.toggle;
    };
    auto sound=featureToggle(extras,L"","@WinUI.AudioReactiveLightbar@","@WinUI.AudioReactiveLightbarDescription@",FeatureOption::AudioToLed);
    Divider(extras);
    // Player LEDs: one drop-down covers both the on/off state and the brightness level.
    auto playerLeds=Choice("@WinUI.PlayerLEDs@",{"@WinUI.Off@","@Low@","@Medium@","@High@"});playerLeds.Header(nullptr);playerLeds.Width(180);
    HeaderRow(extras,L"\uE716","@WinUI.PlayerLEDs@","@WinUI.PlayerLEDsDescription@",playerLeds);
    playerLeds.SelectionChanged([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host){
        const int index=sender.template as<ComboBox>().SelectedIndex();if(index<0||index>3)return;
        if(index>0)self->Submit(self->m_host->Core().SetFeature(self->m_selected,FeatureOption::Brightness,3-index));
        self->Submit(self->m_host->Core().SetPlayerLedEnabled(self->m_selected,index>0));}});
    Divider(extras);
    auto game=featureToggle(extras,L"","@WinUI.UseGameLightbarColor@","@WinUI.GameLightbarDescription@",FeatureOption::EmulatedLight);

    v->bindings.push_back([modes,titles,headers,brightness,percent,battery,cycle,speed,game,staticToggle,color,hex,channels,swatchBrush,pick,presets,colorState,activeName,activeDetails,sound,playerLeds](auto const& s,bool force){
        auto& c=s.controllers[s.selectedController];const bool available=s.running&&c.connected&&!s.udpActive;
        auto value=[&c](FeatureOption option){return c.features[size_t(option)];};
        // Written only on change: these run every frame.
        auto enable=[](Control const& control,bool enabled){if(control.IsEnabled()!=enabled)control.IsEnabled(enabled);};
        auto check=[](ToggleSwitch const& toggle,bool on){if(toggle.IsOn()!=on)toggle.IsOn(on);};
        int activeIndex=-1;for(int i=0;i<3;++i)if(modes[i]==c.baseLightbarMode)activeIndex=i;
        const auto band=BatteryBandForPercent(c.batteryPercent);
        const auto batteryColor=NativeLocalization::Value(band==BatteryLightbarBand::Green?"@WinUI.Green@":band==BatteryLightbarBand::Yellow?"@WinUI.Yellow@":"@WinUI.Red@");
        const auto batteryInfo=band==BatteryLightbarBand::Unavailable?NativeLocalization::Value("@WinUI.BatteryLevelUnavailable@"):std::to_string(c.batteryPercent)+"% · "+batteryColor;
        const char* speedNames[]{"@WinUI.VerySlow@","@WinUI.Slow@","@WinUI.Normal@","@WinUI.Fast@","@WinUI.VeryFast@"};
        const int speedIndex=NearestLightbarCycleSpeed(value(FeatureOption::DiscoSpeed));
        const auto speedInfo=NativeLocalization::Value("@WinUI.ColorCycleSpeed@")+": "+NativeLocalization::Value(speedNames[speedIndex]);
        const auto modeName=NativeLocalization::Value(c.lightbarMode==LightbarMode::Game?"@WinUI.GameLightbarColor@":activeIndex>=0?titles[activeIndex]:"@WinUI.NativeLightbar@");
        const std::array<std::string,3> details{batteryInfo,speedInfo,ColorHex(c.lightbarColor)};
        for(int i=0;i<3;++i) {
            const auto overridden=i==activeIndex&&c.lightbarMode==LightbarMode::Game?NativeLocalization::Value("@WinUI.LightingOverriddenBy@")+" "+modeName:std::string{};
            NativeLocalization::SetText(headers[i].note,details[i]+(overridden.empty()?"":" · "+overridden));
        }
        enable(brightness,available);const int level=int(std::clamp(c.lightbarBrightness,0.f,1.f)*100+0.5f);
        if(int(brightness.Value())!=level)brightness.Value(level);NativeLocalization::SetText(percent,std::to_string(level)+"%");
        enable(battery,available);check(battery,c.batteryBasedLightbarEnabled);
        enable(cycle,available);check(cycle,value(FeatureOption::Disco)!=0);
        if(speed.SelectedIndex()!=speedIndex)speed.SelectedIndex(speedIndex);
        enable(speed,available&&c.baseLightbarMode==BaseLightbarMode::ColorCycle);
        const bool staticMode=c.baseLightbarMode==BaseLightbarMode::Static;
        enable(staticToggle,available);check(staticToggle,staticMode);
        enable(pick,available&&staticMode);for(auto const& preset:*presets)enable(preset,available&&staticMode);
        enable(game,available);check(game,c.gameLightbarOverrideEnabled);
        NativeLocalization::SetText(activeName,modeName);
        NativeLocalization::SetText(activeDetails,c.lightbarMode==LightbarMode::Game?NativeLocalization::Value("@WinUI.GameLightbarHeaderDescription@"):activeIndex>=0?details[activeIndex]:NativeLocalization::Value("@WinUI.NativeLightbarDescription@"));
        if(force||colorState->last!=c.lightbarColor) {
            colorState->last=c.lightbarColor;
            std::array<uint8_t,3> bytes{};for(int i=0;i<3;++i)bytes[i]=uint8_t(std::clamp(c.lightbarColor[i],0.f,1.f)*255+0.5f);
            const Windows::UI::Color shown{255,bytes[0],bytes[1],bytes[2]};
            color.Color(shown);swatchBrush.Color(shown);hex.Text(to_hstring(ColorHex(c.lightbarColor)));
            for(int i=0;i<3;++i)channels[i].Value(bytes[i]);
        }
        check(sound,value(FeatureOption::AudioToLed)!=0);
        enable(sound,available&&(c.lightbarMode==LightbarMode::Static||c.lightbarMode==LightbarMode::ColorCycle));
        const int ledChoice=c.playerLedEnabled?3-std::clamp(int(value(FeatureOption::Brightness)),0,2):0;if(playerLeds.SelectedIndex()!=ledChoice)playerLeds.SelectedIndex(ledChoice);
        enable(playerLeds,available);
    });
}
void FeaturePage::Options(std::shared_ptr<View> const& v,std::string const& section) {
    auto p=Card(section=="Sound & Haptics"||section=="Lighting"?v->right:v->left,"@WinUI.Configuration@");
    // Rows of switches and sliders carry their own height; keep them close, with air only under the title.
    p.Spacing(2);p.Children().GetAt(0).as<FrameworkElement>().Margin(Thickness{0,0,0,10});
    if(section=="Adaptive Triggers")p.Children().Append(Text("@WinUI.HapticsToTriggersRequiresAudioPassthroughStaticEffectsAndDynamicEffectsShareTheSelectedControllerSTriggerOutput@"));
    for(auto const& d:FeatureDescriptors())if(section==d.section){
        if(d.id==FeatureOption::Wasd||d.id==FeatureOption::PsWindows)continue;
        if(d.id==FeatureOption::AudioPath||d.id==FeatureOption::Brightness||d.id==FeatureOption::HapticsMode) {
            auto items=d.id==FeatureOption::AudioPath?std::vector<std::string>{"@StereoHeadset@","@MonoLeftHeadset@","@WinUI.MonoHeadsetAndSpeaker@","@WinUI.Speaker@"}:d.id==FeatureOption::Brightness?std::vector<std::string>{"@High@","@Medium@","@Low@"}:std::vector<std::string>{"@Vibration@","@HapticsAtModeResistance@"};
            const std::string label=d.id==FeatureOption::AudioPath?"@WinUI.AudioOutput@":d.id==FeatureOption::Brightness?"@WinUI.Brightness@":"@WinUI.HapticsTriggerMode@";
            auto choice=Choice(label,items);choice.Header(nullptr);choice.Width(220);SettingRow(p,label,choice);
            choice.SelectionChanged([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,sender.template as<ComboBox>().SelectedIndex()));});
            v->bindings.push_back([choice,id=d.id,last=-1](auto const& s,bool force)mutable{int value=int(s.controllers[s.selectedController].features[size_t(id)]);if(force||last!=value)choice.SelectedIndex(value);last=value;});
        }else if(d.minimum==0&&d.maximum==1){auto toggle=RowToggle();SettingRow(p,d.label,toggle);
            toggle.Toggled([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,sender.template as<ToggleSwitch>().IsOn()?1:0));});
            v->bindings.push_back([toggle,id=d.id,last=-1](auto const& s,bool force)mutable{int val=int(s.controllers[s.selectedController].features[size_t(id)]);if(force||last!=val)toggle.IsOn(val!=0);last=val;});
        }else {auto range=Range(d.minimum,d.maximum,d.step);SettingRow(p,d.label,range.root);Automation::AutomationProperties::SetName(range.slider,to_hstring(d.label));
            range.slider.ValueChanged([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host){const double value=sender.template as<Slider>().Value();if(std::isfinite(value))self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,value));}});
            v->bindings.push_back([slider=range.slider,text=range.value,id=d.id,step=d.step,last=std::numeric_limits<double>::quiet_NaN()](auto const& s,bool force)mutable{double val=s.controllers[s.selectedController].features[size_t(id)];
                if(force||last!=val){slider.Value(val);text.Text(RangeText(val,step));}last=val;});
        }
    }
    v->bindings.push_back([p,section](auto const& s,bool){Enable(p,s.running&&s.controllers[s.selectedController].connected&&(!(section=="Lighting"||section=="Adaptive Triggers")||!s.udpActive));});
}
void FeaturePage::RebuildTrigger(TriggerEditor& e,bool defaults) {
    NativeLocalization::Invalidate();
    e.parameters.Children().Clear();e.numbers.clear();int mode=e.mode.SelectedIndex();auto& modes=TriggerModes(e.format.SelectedIndex()==1);
    if(mode<0||mode>=int(modes.size()))return;
    for(size_t k=0;k<modes[mode].parameters.size();++k){auto& d=modes[mode].parameters[k];auto n=Number(d.name,d.minimum,d.maximum);n.Value(defaults?d.minimum:std::clamp(e.last.parameters[k],d.minimum,d.maximum));e.numbers.push_back(n);
        if(e.format.SelectedIndex()==1&&mode==11&&k==0){auto choice=Choice("@WinUI.CustomTriggerMode@",{"@WinUI.Off@","@WinUI.Rigid@","@WinUI.RigidA@","@WinUI.RigidB@","@WinUI.RigidAB@","@WinUI.Pulse@","@WinUI.PulseA@","@WinUI.PulseB@","@WinUI.PulseAB@"});choice.SelectedIndex(int(n.Value()));choice.SelectionChanged([n](auto const& sender,auto&&){if(NativeLocalization::applying)return;n.Value(sender.template as<ComboBox>().SelectedIndex());});e.parameters.Children().Append(choice);}
        else e.parameters.Children().Append(n);}
}
void FeaturePage::Triggers(std::shared_ptr<View> const& v) {
    auto card=Card(v->left,"");
    Grid header;header.ColumnSpacing(16);header.RowSpacing(8);
    for(int i=0;i<2;++i){header.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());header.RowDefinitions().Append(row);}
    auto title=Text("@WinUI.TriggerEffects@",20);title.VerticalAlignment(VerticalAlignment::Center);header.Children().Append(title);card.Children().Append(header);
    ResponsivePageLayout::Attach(header,1,1,620);
    Grid copy;copy.ColumnSpacing(8);copy.RowSpacing(8);for(int i=0;i<2;++i){copy.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());copy.RowDefinitions().Append(row);}Grid::SetColumn(copy,1);header.Children().Append(copy);copy.HorizontalAlignment(HorizontalAlignment::Right);copy.VerticalAlignment(VerticalAlignment::Center);
    for(int source=0;source<2;++source){auto button=Action(source?"@WinUI.CopyR2ToL2@":"@WinUI.CopyL2ToR2@",[weakView=std::weak_ptr<View>(v),source](auto& self){
        if(auto view=weakView.lock();view&&self.m_host){auto& editor=view->triggers[source];std::array<int,11> params{};
            for(size_t k=0;k<editor.numbers.size();++k){auto value=editor.numbers[k].Value();if(!std::isfinite(value)||std::floor(value)!=value){self.Submit(0);return;}params[k]=int(value);}
            self.Submit(self.m_host->Core().SetTrigger(self.m_selected,1-source,editor.format.SelectedIndex()==1,editor.mode.SelectedIndex(),params));}
    });button.Padding(Thickness{10,6,10,6});Grid::SetColumn(button,source);copy.Children().Append(button);v->bindings.push_back([button](auto const& s,bool){auto& c=s.controllers[s.selectedController];button.IsEnabled(s.running&&c.connected&&c.type==ControllerType::DualSense&&!s.udpActive);});}
    Grid effects;effects.ColumnSpacing(20);effects.RowSpacing(20);
    for(int i=0;i<2;++i){effects.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());effects.RowDefinitions().Append(row);}
    card.Children().Append(effects);ResponsivePageLayout::Attach(effects,1,1,600);
    for(int side=0;side<2;++side){
        Grid column;column.RowSpacing(16);RowDefinition configuration;configuration.Height(GridLengthHelper::FromValueAndType(1,GridUnitType::Star));column.RowDefinitions().Append(configuration);RowDefinition actions;actions.Height(GridLengthHelper::Auto());column.RowDefinitions().Append(actions);
        Panel p,footer;p.Spacing(12);footer.Spacing(12);p.VerticalAlignment(VerticalAlignment::Top);
        column.Children().Append(p);Grid::SetRow(footer,1);column.Children().Append(footer);Grid::SetColumn(column,side);effects.Children().Append(column);auto& e=v->triggers[side];
        auto label=Text(side?"R2":"L2",18);label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());p.Children().Append(label);
        auto status=Text("");status.Opacity(0.7);status.TextWrapping(TextWrapping::NoWrap);status.TextTrimming(TextTrimming::CharacterEllipsis);p.Children().Append(status);
        v->bindings.push_back([status,side](auto const& s,bool){auto& c=s.controllers[s.selectedController];status.Text(to_hstring(NativeLocalization::Value("@WinUI.ConfiguredEffect@")+": "+NativeLocalization::Value(c.triggers[side].mode)));});
        e.format=Choice("@WinUI.Format@",{"Sony","DSX"});e.mode=Choice("@WinUI.Effect@",{});e.parameters.Spacing(12);
        p.Children().Append(e.format);p.Children().Append(e.mode);p.Children().Append(e.parameters);
        auto weakView=std::weak_ptr<View>(v);
        e.format.SelectionChanged([weak=get_weak(),weakView,side](auto&&,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying)if(auto view=weakView.lock()){
            self->m_updating=true;auto& editor=view->triggers[side];editor.mode.Items().Clear();for(auto& mode:TriggerModes(editor.format.SelectedIndex()==1)){ComboBoxItem entry;entry.Content(box_value(to_hstring(mode.name)));editor.mode.Items().Append(entry);}editor.mode.SelectedIndex(0);self->RebuildTrigger(editor,true);self->m_updating=false;}});
        e.mode.SelectionChanged([weak=get_weak(),weakView,side](auto&&,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying)if(auto view=weakView.lock())self->RebuildTrigger(view->triggers[side],true);});
        Button(footer,"@WinUI.ApplyEffect@",[weakView,side](auto& self){if(auto view=weakView.lock();view&&self.m_host){auto& editor=view->triggers[side];std::array<int,11> params{};
            for(size_t k=0;k<editor.numbers.size();++k){double value=editor.numbers[k].Value();if(!std::isfinite(value)||std::floor(value)!=value){self.Submit(0);return;}params[k]=int(value);}
            self.Submit(self.m_host->Core().SetTrigger(self.m_selected,side,editor.format.SelectedIndex()==1,editor.mode.SelectedIndex(),params));}});
        Panel live;live.Spacing(6);footer.Children().Append(live);
        auto input=Text("",12),haptic=Text("",12);input.Opacity(0.7);haptic.Opacity(0.7);
        ProgressBar inputBar,hapticBar;inputBar.Maximum(255);hapticBar.Maximum(1);
        live.Children().Append(input);live.Children().Append(inputBar);live.Children().Append(haptic);live.Children().Append(hapticBar);
        v->bindings.push_back([input,haptic,inputBar,hapticBar,side](auto const& s,bool){auto& c=s.controllers[s.selectedController];
            input.Text(to_hstring(NativeLocalization::Value("@WinUI.TriggerInput@")+": "+(c.connected?std::to_string(int(c.analogTriggers[side]*100/255.0+0.5))+"%":"\u2014")));
            haptic.Text(to_hstring(NativeLocalization::Value("@WinUI.TriggerHaptic@")+": "+(c.connected?std::to_string(int(std::clamp(c.hapticLevels[side],0.f,1.f)*100+0.5))+"%":"\u2014")));
            inputBar.Value(c.connected?c.analogTriggers[side]:0);hapticBar.Value(c.connected?c.hapticLevels[side]:0);});
        v->bindings.push_back([weak=get_weak(),weakView,side,p,footer](auto const& s,bool force){auto self=weak.get();auto view=weakView.lock();if(!self||!view)return;auto& e=view->triggers[side];auto& c=s.controllers[s.selectedController];auto const& t=c.triggers[side];
            if(force||!e.initialized||e.last.format!=t.format||e.last.mode!=t.mode||e.last.parameters!=t.parameters){e.initialized=true;e.last=t;e.format.SelectedIndex(t.format=="DSX"?1:0);e.mode.Items().Clear();int selected=0,k=0;for(auto& mode:TriggerModes(t.format=="DSX")){ComboBoxItem entry;entry.Content(box_value(to_hstring(mode.name)));e.mode.Items().Append(entry);if(mode.name==t.mode)selected=k;++k;}e.mode.SelectedIndex(selected);self->RebuildTrigger(e,false);}const bool available=s.running&&c.connected&&c.type==ControllerType::DualSense&&!s.udpActive;Enable(p,available);Enable(footer,available);});
    }
    TriggerDivider(effects,600);
}
void FeaturePage::TriggerBehavior(std::shared_ptr<View> const& v) {
    auto card=Card(v->left,"@WinUI.TriggerBehavior@");
    auto note=Text("@WinUI.HapticsToTriggersRequiresAudioPassthroughStaticEffectsAndDynamicEffectsShareTheSelectedControllerSTriggerOutput@",12);note.Opacity(0.7);card.Children().Append(note);
    std::array<Panel,3> groups;
    for(int i=0;i<3;++i){groups[i].Spacing(12);auto title=Text(i==0?"@WinUI.HapticTriggerRouting@":i==1?"@WinUI.TriggerButtonBehavior@":"@WinUI.RumbleToTriggers@",16);title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());groups[i].Children().Append(title);}
    Grid routing;routing.ColumnSpacing(20);routing.RowSpacing(20);
    for(int i=0;i<2;++i){routing.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());routing.RowDefinitions().Append(row);}
    Panel hapticAndButtons;hapticAndButtons.Spacing(20);hapticAndButtons.Children().Append(groups[0]);hapticAndButtons.Children().Append(groups[1]);
    routing.Children().Append(hapticAndButtons);Grid::SetColumn(groups[2],1);routing.Children().Append(groups[2]);card.Children().Append(routing);
    ResponsivePageLayout::Attach(routing,1,1,700);TriggerDivider(routing,700);
    Grid rumble;rumble.ColumnSpacing(20);rumble.RowSpacing(16);std::array<Panel,2> sides;
    for(int i=0;i<2;++i){rumble.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());rumble.RowDefinitions().Append(row);sides[i].Spacing(12);Grid::SetColumn(sides[i],i);rumble.Children().Append(sides[i]);}
    ResponsivePageLayout::Attach(rumble,1,1,520);
    auto settingRow=[](Panel const& parent,std::string const& label,FrameworkElement const& control){SettingRow(parent,label,control);};
    for(auto const& d:FeatureDescriptors())if(std::string_view(d.section)=="Adaptive Triggers") {
        auto parent=int(d.id)<=int(FeatureOption::HapticsSwap)?groups[0]:int(d.id)<=int(FeatureOption::ButtonPosition)?groups[1]:groups[2];
        if(int(d.id)>=int(FeatureOption::RumbleFrequencyL))parent=sides[(int(d.id)-int(FeatureOption::RumbleFrequencyL))%2];
        auto available=[](auto const& s){auto& c=s.controllers[s.selectedController];return s.running&&c.connected&&!s.udpActive;};
        if(d.id==FeatureOption::HapticsMode){auto choice=Choice("@WinUI.HapticsTriggerMode@",{"@Vibration@","@HapticsAtModeResistance@"});choice.Header(nullptr);choice.Width(160);settingRow(parent,"@WinUI.HapticsTriggerMode@",choice);
            choice.SelectionChanged([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,sender.template as<ComboBox>().SelectedIndex()));});
            v->bindings.push_back([choice,id=d.id,available,last=-1](auto const& s,bool force)mutable{int value=int(s.controllers[s.selectedController].features[size_t(id)]);if(force||last!=value)choice.SelectedIndex(value);last=value;choice.IsEnabled(available(s));});
        }else if(d.minimum==0&&d.maximum==1){ToggleSwitch toggle;toggle.MinWidth(44);toggle.Width(44);
            auto hiddenState=Markup::XamlReader::Load(L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'><Grid Visibility='Collapsed'/></DataTemplate>").as<DataTemplate>();
            toggle.OnContentTemplate(hiddenState);toggle.OffContentTemplate(hiddenState);settingRow(parent,d.label,toggle);
            toggle.Toggled([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,sender.template as<ToggleSwitch>().IsOn()?1:0));});
            v->bindings.push_back([toggle,id=d.id,available,last=-1](auto const& s,bool force)mutable{int value=int(s.controllers[s.selectedController].features[size_t(id)]);if(force||last!=value)toggle.IsOn(value!=0);last=value;toggle.IsEnabled(available(s));});
        }else{auto number=Number(d.label,d.minimum,d.maximum,d.step);number.Header(nullptr);number.Width(160);settingRow(parent,d.label,number);
            number.ValueChanged([weak=get_weak(),id=d.id](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host&&std::isfinite(sender.Value()))self->Submit(self->m_host->Core().SetFeature(self->m_selected,id,sender.Value()));});
            v->bindings.push_back([number,id=d.id,available,last=std::numeric_limits<double>::quiet_NaN()](auto const& s,bool force)mutable{double value=s.controllers[s.selectedController].features[size_t(id)];if(force||last!=value)number.Value(value);last=value;number.IsEnabled(available(s));});
        }
    }
    groups[2].Children().Append(rumble);
}
void FeaturePage::MotionTelemetry(std::shared_ptr<View> const& v) {
    auto card=Card(v->right,"@WinUI.MotionAndTouchFeedback@");
    Grid data;data.ColumnSpacing(20);data.RowSpacing(20);
    card.Children().Append(data);
    std::array<Panel,4> groups;const char* names[]{"@WinUI.TelemetryGyroscopeTitle@","@WinUI.TelemetryAccelerationTitle@","@WinUI.TelemetryTouchTitle@1","@WinUI.TelemetryTouchTitle@2"};
    for(int i=0;i<4;++i){groups[i].Spacing(8);auto title=Text(names[i]);title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());groups[i].Children().Append(title);data.Children().Append(groups[i]);}
    auto field=[](Panel const& group,std::string const& label) {
        Grid row;row.ColumnSpacing(8);ColumnDefinition key;key.Width(GridLengthHelper::FromValueAndType(52,GridUnitType::Pixel));row.ColumnDefinitions().Append(key);row.ColumnDefinitions().Append(ColumnDefinition());
        auto name=Text(label);name.Opacity(0.7);row.Children().Append(name);auto value=Text("");Grid::SetColumn(value,1);row.Children().Append(value);group.Children().Append(row);return value;
    };
    std::array<TextBlock,6> axes;std::array<TextBlock,4> touchCoordinates;std::array<TextBlock,2> touchStates,touchIds;
    const char* axisNames[]{"X","Y","Z"};
    for(int side=0;side<2;++side){for(int axis=0;axis<3;++axis)axes[side*3+axis]=field(groups[side],axisNames[axis]);touchStates[side]=field(groups[side+2],"@WinUI.InputState@");for(int axis=0;axis<2;++axis)touchCoordinates[side*2+axis]=field(groups[side+2],axisNames[axis]);touchIds[side]=field(groups[side+2],"ID");}
    if(m_name=="Diagnostics")AdaptiveColumns(data,4,720,400);else AdaptiveColumns(data,2,280,280);
    v->bindings.push_back([axes,touchCoordinates,touchStates,touchIds,tick=0](auto const& s,bool force)mutable{auto& c=s.controllers[s.selectedController];
        if(!force&&tick++%3)return;
        auto decimal=[](double value){std::ostringstream text;text<<std::fixed<<std::setprecision(2)<<value;return to_hstring(text.str());};
        for(int axis=0;axis<3;++axis){axes[axis].Text(c.connected?decimal(c.angularVelocity[axis]):L"\u2014");axes[axis+3].Text(c.connected?decimal(c.acceleration[axis]):L"\u2014");}
        for(int side=0;side<2;++side){touchStates[side].Text(c.connected?to_hstring(NativeLocalization::Value(c.touches[side].active?"@WinUI.Active@":"@WinUI.Released@")):L"\u2014");touchCoordinates[side*2].Text(c.connected?to_hstring(c.touches[side].x):L"\u2014");touchCoordinates[side*2+1].Text(c.connected?to_hstring(c.touches[side].y):L"\u2014");touchIds[side].Text(c.connected?to_hstring(c.touches[side].id):L"\u2014");}
    });
}
void FeaturePage::TriggerPresets(std::shared_ptr<View> const& v) {
    auto card=Card(v->right,"@WinUI.EffectPresets@");card.Spacing(20);
    card.Children().Append(Text("@WinUI.TriggerPresetDescription@",12));
    auto target=std::make_shared<int>(0);
    Panel targetGroup;targetGroup.Spacing(12);targetGroup.Orientation(Orientation::Horizontal);auto targetLabel=Text("@WinUI.ApplyPresetTo@");targetLabel.VerticalAlignment(VerticalAlignment::Center);targetGroup.Children().Append(targetLabel);
    Panel choices;choices.Orientation(Orientation::Horizontal);choices.Spacing(16);targetGroup.Children().Append(choices);
    targetGroup.SizeChanged([choices](auto const& sender,SizeChangedEventArgs const& args){sender.as<StackPanel>().Orientation(args.NewSize().Width<560?Orientation::Vertical:Orientation::Horizontal);choices.Orientation(args.NewSize().Width<360?Orientation::Vertical:Orientation::Horizontal);});
    const char* targets[]{"@WinUI.BothTriggers@","@WinUI.L2Only@","@WinUI.R2Only@"};
    for(int i=0;i<3;++i){RadioButton choice;choice.Content(box_value(to_hstring(targets[i])));choice.GroupName(L"AdaptiveTriggerPresetTarget");choice.IsChecked(i==0);choices.Children().Append(choice);
        choice.Checked([target,i](auto&&,auto&&){*target=i;});}
    Grid tiles;tiles.ColumnSpacing(8);tiles.RowSpacing(8);
    card.Children().Append(tiles);
    auto selected=std::make_shared<int>(0);auto changing=std::make_shared<bool>(false);auto buttons=std::make_shared<std::array<Primitives::ToggleButton,4>>();
    const char* titles[]{"@WinUI.Off@","@WinUI.Soft@","@Medium@","@WinUI.Hard@"};
    const wchar_t* icons[]{L"\uE711",L"\uEC48",L"\uEC49",L"\uEC4A"}; // Cancel, then low/medium/high gauge: increasing resistance.
    for(int i=0;i<4;++i){auto button=(*buttons)[i];button.HorizontalAlignment(HorizontalAlignment::Stretch);button.HorizontalContentAlignment(HorizontalAlignment::Stretch);button.Padding(Thickness{12,12,12,12});button.Height(48);button.VerticalAlignment(VerticalAlignment::Stretch);button.VerticalContentAlignment(VerticalAlignment::Center);button.BorderThickness(Thickness{1,1,1,1});button.CornerRadius(Microsoft::UI::Xaml::CornerRadius{8,8,8,8});
        Grid content;content.ColumnSpacing(12);ColumnDefinition iconColumn;iconColumn.Width(GridLengthHelper::Auto());content.ColumnDefinitions().Append(iconColumn);content.ColumnDefinitions().Append(ColumnDefinition());
        FontIcon icon;icon.Glyph(icons[i]);icon.FontSize(20);icon.VerticalAlignment(VerticalAlignment::Center);content.Children().Append(icon);
        auto title=Text(titles[i]);title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());title.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(title,1);content.Children().Append(title);button.Content(content);Automation::AutomationProperties::SetName(button,to_hstring(titles[i]));
        tiles.Children().Append(button);button.IsChecked(i==0);
        button.Click([selected,changing,weakButtons=std::weak_ptr<std::array<Primitives::ToggleButton,4>>(buttons),i](auto&&,auto&&){if(*changing)return;if(auto all=weakButtons.lock()){*changing=true;*selected=i;for(int k=0;k<4;++k)(*all)[k].IsChecked(k==i);*changing=false;}});
    }
    AdaptiveColumns(tiles,4,640,360);
    auto apply=Action("@Apply@",[selected,target](auto& self){if(!self.m_host)return;const int modes[]{0,3,4,5};std::array<int,11> params{};
        const int first=*target==2?1:0,last=*target==1?0:1;std::vector<ApplicationCore::RequestId> results;
        for(int side=first;side<=last;++side)results.push_back(self.m_host->Core().SetTrigger(self.m_selected,side,*selected!=0,modes[*selected],params));
        self.m_notifications.SubmitBatch(results);});
    Grid actions;actions.ColumnSpacing(20);actions.RowSpacing(12);actions.ColumnDefinitions().Append(ColumnDefinition());ColumnDefinition buttonColumn;buttonColumn.Width(GridLengthHelper::Auto());actions.ColumnDefinitions().Append(buttonColumn);
    RowDefinition actionRow;actionRow.Height(GridLengthHelper::Auto());actions.RowDefinitions().Append(actionRow);
    targetGroup.VerticalAlignment(VerticalAlignment::Center);actions.Children().Append(targetGroup);apply.HorizontalAlignment(HorizontalAlignment::Right);apply.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(apply,1);actions.Children().Append(apply);card.Children().Append(actions);
    actions.SizeChanged([apply](auto const& sender,SizeChangedEventArgs const& args){auto layout=sender.as<Grid>();const bool narrow=args.NewSize().Width<460;
        if(narrow&&layout.RowDefinitions().Size()==1){RowDefinition row;row.Height(GridLengthHelper::Auto());layout.RowDefinitions().Append(row);}
        Grid::SetColumn(apply,narrow?0:1);Grid::SetRow(apply,narrow?1:0);
        if(!narrow&&layout.RowDefinitions().Size()>1)layout.RowDefinitions().RemoveAtEnd();});
    v->bindings.push_back([buttons,apply](auto const& s,bool){auto& c=s.controllers[s.selectedController];bool available=s.running&&c.connected&&c.type==ControllerType::DualSense&&!s.udpActive;apply.IsEnabled(available);for(auto button:*buttons)button.IsEnabled(available);});
}
void FeaturePage::DeadzoneTelemetry(std::shared_ptr<View> const& v) {
    auto card=Card(v->right,"@WinUI.StickDeadzoneComparison@");
    auto description=Text("@WinUI.DeadzonePreviewDescription@",12);description.Opacity(0.7);card.Children().Append(description);
    std::array<TextBlock,2> deadzones;std::array<TextBlock,8> values;
    for(int side=0;side<2;++side) {
        Panel group;group.Spacing(8);auto title=Text(side?"@WinUI.RightStick@":"@WinUI.LeftStick@");title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());group.Children().Append(title);
        Grid setting;setting.ColumnSpacing(12);setting.ColumnDefinitions().Append(ColumnDefinition());ColumnDefinition number;number.Width(GridLengthHelper::Auto());setting.ColumnDefinitions().Append(number);
        auto label=Text("@WinUI.ConfiguredDeadzone@");label.Opacity(0.7);setting.Children().Append(label);Grid::SetColumn(deadzones[side],1);setting.Children().Append(deadzones[side]);group.Children().Append(setting);
        Grid comparison;comparison.ColumnSpacing(12);comparison.RowSpacing(8);
        for(int column=0;column<3;++column)comparison.ColumnDefinitions().Append(ColumnDefinition());
        for(int row=0;row<3;++row){RowDefinition definition;definition.Height(GridLengthHelper::Auto());comparison.RowDefinitions().Append(definition);}
        for(int axis=0;axis<2;++axis){auto heading=Text(axis?"Y":"X",12);heading.Opacity(0.7);Grid::SetColumn(heading,axis+1);comparison.Children().Append(heading);}
        const char* labels[]{"@WinUI.RawInput@","@WinUI.DeadzonePreview@"};
        for(int row=0;row<2;++row){auto name=Text(labels[row],12);name.Opacity(0.7);Grid::SetRow(name,row+1);comparison.Children().Append(name);
            for(int axis=0;axis<2;++axis){auto value=values[side*4+row*2+axis];Grid::SetColumn(value,axis+1);Grid::SetRow(value,row+1);comparison.Children().Append(value);}}
        group.Children().Append(comparison);card.Children().Append(group);
    }
    v->bindings.push_back([deadzones,values](auto const& s,bool){auto& c=s.controllers[s.selectedController];
        for(int side=0;side<2;++side){const int deadzone=side?c.rightDeadzone:c.leftDeadzone;deadzones[side].Text(to_hstring(deadzone));
            const int x=c.stickAxes[side*2],y=c.stickAxes[side*2+1],dx=x-128,dy=y-128;
            // Snapshot-only preview of the existing radial deadzone, not the final emulated output.
            const bool centered=deadzone>0&&dx*dx+dy*dy<=deadzone*deadzone;
            for(int axis=0;axis<2;++axis){const int raw=axis?y:x;
                values[side*4+axis].Text(c.connected?to_hstring(raw):L"\u2014");values[side*4+axis+2].Text(c.connected?to_hstring(centered?128:raw):L"\u2014");}
        }
    });
}
void FeaturePage::Telemetry(std::shared_ptr<View> const& v) {
    if(m_name=="Motion & Touch"){MotionTelemetry(v);return;}
    if(m_name=="Sound & Haptics") {
        auto card=Card(v->left,"@WinUI.AudioAndHapticFeedback@");std::array<ProgressBar,3> bars;std::array<TextBlock,3> values;
        const char* labels[]{"@WinUI.TelemetryAudioPeakTitle@","@WinUI.LeftHaptics@","@WinUI.RightHaptics@"};
        for(int i=0;i<3;++i){Panel group;group.Spacing(8);Grid row;row.ColumnSpacing(12);row.ColumnDefinitions().Append(ColumnDefinition());ColumnDefinition valueColumn;valueColumn.Width(GridLengthHelper::Auto());row.ColumnDefinitions().Append(valueColumn);
            row.Children().Append(Text(labels[i]));Grid::SetColumn(values[i],1);row.Children().Append(values[i]);group.Children().Append(row);
            bars[i].Maximum(1);Automation::AutomationProperties::SetName(bars[i],to_hstring(labels[i]));group.Children().Append(bars[i]);card.Children().Append(group);}
        v->bindings.push_back([bars,values](auto const& s,bool){auto& c=s.controllers[s.selectedController];const float levels[]{c.capturePeak,c.hapticLevels[0],c.hapticLevels[1]};
            for(int i=0;i<3;++i){std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<levels[i];values[i].Text(c.connected?to_hstring(out.str()):L"\u2014");bars[i].Value(c.connected?levels[i]:0);bars[i].Opacity(c.connected?1:0.5);}
        });return;
    }
    auto p=Card(v->left,"@WinUI.LiveInputAndHaptics@");auto info=Text("");info.IsTextSelectionEnabled(true);p.Children().Append(info);
    // Sticks are drawn as position dots; triggers and haptics as level bars. Motion and touch have their own table.
    auto brush=[](wchar_t const* key){return Application::Current().Resources().Lookup(box_value(key)).as<Media::Brush>();};
    const auto outline=brush(L"TextFillColorSecondaryBrush"),grid=brush(L"CardStrokeColorDefaultBrush"),accent=brush(L"AccentFillColorDefaultBrush");
    Grid sticks;sticks.ColumnSpacing(20);sticks.ColumnDefinitions().Append(ColumnDefinition());sticks.ColumnDefinitions().Append(ColumnDefinition());p.Children().Append(sticks);
    std::array<Shapes::Ellipse,2> dots;std::array<TextBlock,2> positions;
    for(int side=0;side<2;++side) {
        Panel group;group.Spacing(4);Grid::SetColumn(group,side);sticks.Children().Append(group);
        auto title=Text(side?"@WinUI.RightStick@":"@WinUI.LeftStick@");title.HorizontalAlignment(HorizontalAlignment::Center);group.Children().Append(title);
        Canvas canvas;canvas.Width(140);canvas.Height(140);canvas.IsHitTestVisible(false);
        for(double size:{120.0,60.0}){Shapes::Ellipse ring;ring.Width(size);ring.Height(size);ring.Stroke(size>100?outline:grid);ring.StrokeThickness(1);Canvas::SetLeft(ring,(140-size)/2);Canvas::SetTop(ring,(140-size)/2);canvas.Children().Append(ring);}
        for(int axis=0;axis<2;++axis){Shapes::Line line;line.X1(axis?10:70);line.Y1(axis?70:10);line.X2(axis?130:70);line.Y2(axis?70:130);line.Stroke(grid);canvas.Children().Append(line);}
        dots[side].Width(12);dots[side].Height(12);dots[side].Fill(accent);Canvas::SetLeft(dots[side],64);Canvas::SetTop(dots[side],64);canvas.Children().Append(dots[side]);
        Viewbox box;box.MaxHeight(130);box.Child(canvas);group.Children().Append(box);
        positions[side].FontSize(12);positions[side].Opacity(0.7);positions[side].HorizontalAlignment(HorizontalAlignment::Center);group.Children().Append(positions[side]);
    }
    std::array<ProgressBar,4> bars;const char* labels[]={"L2","R2","@WinUI.LeftHaptics@","@WinUI.RightHaptics@"};
    for(int k=0;k<4;++k){Panel group;group.Spacing(6);group.Children().Append(Text(labels[k]));bars[k].Maximum(k<2?255:1);Automation::AutomationProperties::SetName(bars[k],to_hstring(labels[k]));group.Children().Append(bars[k]);p.Children().Append(group);}
    v->bindings.push_back([info,bars,dots,positions,tick=0](auto const& s,bool force)mutable{auto& c=s.controllers[s.selectedController];
        const auto place=[&c](int axis){return c.connected?64+54*std::clamp((axis-128)/127.0,-1.0,1.0):64.0;};
        for(int side=0;side<2;++side){Canvas::SetLeft(dots[side],place(c.stickAxes[side*2]));Canvas::SetTop(dots[side],place(c.stickAxes[side*2+1]));}
        for(int k=0;k<2;++k){bars[k].Value(c.connected?c.analogTriggers[k]:0);bars[k+2].Value(c.connected?c.hapticLevels[k]:0);}
        // Dots and bars follow every frame; text is unreadable above ~10 Hz and costly to lay out.
        if(!force&&tick++%3)return;
        for(int side=0;side<2;++side)NativeLocalization::SetText(positions[side],c.connected?"@WinUI.X@"+std::to_string(c.stickAxes[side*2])+"@WinUI.Y@"+std::to_string(c.stickAxes[side*2+1]):std::string("—"));
        std::ostringstream out;out<<std::fixed<<std::setprecision(2);
        out<<(c.connected?"@WinUI.Connected@":"@Disconnected@")<<"@WinUI.Buttons@"<<(c.buttonNames.empty()?"@None@":c.buttonNames)<<"@WinUI.AudioPeak@"<<c.capturePeak;
        NativeLocalization::SetText(info,out.str());});
}
void FeaturePage::Update(ApplicationSnapshot const& s) {
    m_selected=s.selectedController;if(!m_view)return;m_updating=true;
    bool force=m_view->slot!=m_selected;m_view->slot=m_selected;
    for(auto& update:m_view->bindings)update(s,force);m_updating=false;
    if(m_capture>=0){if(m_selected!=m_captureSlot||!s.controllers[m_captureSlot].connected){m_capture=-1;m_notifications.Show(L"@WinUI.ConnectAControllerToApplyTheseSettingsToADevice@",InfoBarSeverity::Warning);}
        else if(std::chrono::steady_clock::now()>=m_captureEnd){auto buttons=s.controllers[m_captureSlot].buttons;int target=m_capture;m_capture=-1;if(buttons)Submit(m_host->Core().SetHotkey(m_captureSlot,HotkeyTarget(target),buttons));else m_notifications.Show(L"@WinUI.NoControllerButtonsDetectedTryCaptureAgain@",InfoBarSeverity::Warning);}
    }
}
}

namespace winrt::DualsenseZNative::implementation {
void FeaturePage::UdpStatus(std::shared_ptr<View> const& v) {
    // Game mods talk to the local UDP port; nothing here leaves this computer.
    auto p=Card(v->right,"@WinUI.UDPMods@");
    auto status=Text("");p.Children().Append(status);
    auto port=Text("");p.Children().Append(port);
    v->bindings.push_back([status,port](auto const& s,bool){
        NativeLocalization::SetText(status,std::string("@WinUI.UDPModStatus@: ")+(s.udpActive?"@WinUI.Active@":"@WinUI.Inactive@"));
        NativeLocalization::SetText(port,"@WinUI.UDPListeningPort@: "+(s.udpPort>0?std::to_string(s.udpPort):std::string("\u2014")));});
    auto note=Text("@WinUI.UDPModsDescription@",12);note.Opacity(0.7);p.Children().Append(note);
}

}

namespace winrt::DualsenseZNative::implementation {
void FeaturePage::Profiles(std::shared_ptr<View> const& v) {
    // Profiles lays out its own full-width cards instead of the two-column dashboard.
    v->content.Children().RemoveAt(v->content.Children().Size()-1);
    auto secondary=[](Panel const& panel,std::string const& text){auto block=Text(text,12);block.Opacity(0.7);panel.Children().Append(block);return block;};

    auto local=Card(v->content,"@WinUI.LocalProfiles@");local.Spacing(12);
    secondary(local,"@WinUI.LocalProfilesDescription@");
    Panel current;current.Spacing(2);local.Children().Append(current);
    secondary(current,"@WinUI.CurrentProfile@");
    auto profileName=Text("");profileName.FontWeight(Windows::UI::Text::FontWeights::SemiBold());current.Children().Append(profileName);
    auto profilePath=secondary(current,"");
    Grid actions;actions.ColumnSpacing(8);actions.RowSpacing(8);local.Children().Append(actions);
    auto action=[this,&actions](wchar_t const* glyph,std::string const& label,std::function<void(FeaturePage&)> run){
        auto button=Action(label,std::move(run));
        Panel content;content.Orientation(Orientation::Horizontal);content.Spacing(8);content.HorizontalAlignment(HorizontalAlignment::Center);
        FontIcon icon;icon.Glyph(glyph);icon.FontSize(16);content.Children().Append(icon);
        auto text=Text(label);text.TextWrapping(TextWrapping::NoWrap);text.TextTrimming(TextTrimming::CharacterEllipsis);content.Children().Append(text);
        button.Content(content);button.HorizontalAlignment(HorizontalAlignment::Stretch);button.Padding(Thickness{12,10,12,10});
        Automation::AutomationProperties::SetName(button,to_hstring(label));
        actions.Children().Append(button);return button;
    };
    auto load=action(L"\uE8B7","@WinUI.LoadProfile@",[](auto& self){self.PickProfile(0);});
    auto save=action(L"\uE74E","@WinUI.SaveProfileAs@",[](auto& self){self.PickProfile(1);});
    auto setDefault=action(L"\uE734","@WinUI.SetAsDefault@",[](auto& self){self.PickProfile(2);});
    auto removeDefault=action(L"\uE74D","@WinUI.RemoveDefaultProfile@",[](auto& self){if(self.m_host)self.Submit(self.m_host->Core().RemoveDefaultProfile(self.m_selected));});
    AdaptiveColumns(actions,4,760,380);
    v->bindings.push_back([profileName,profilePath,load,save,setDefault,removeDefault](auto const& s,bool){
        auto& c=s.controllers[s.selectedController];
        NativeLocalization::SetText(profileName,c.profilePath.empty()?std::string("@WinUI.CurrentSettingsNotSaved@"):std::filesystem::path(c.profilePath).stem().string());
        NativeLocalization::SetText(profilePath,c.profilePath);
        const auto shown=c.profilePath.empty()?Visibility::Collapsed:Visibility::Visible;if(profilePath.Visibility()!=shown)profilePath.Visibility(shown);
        for(auto const& button:{load,save,setDefault,removeDefault})if(button.IsEnabled()!=s.running)button.IsEnabled(s.running);
    });

    auto games=Card(v->content,"@WinUI.GameProfiles@");games.Spacing(12);
    secondary(games,"@WinUI.GameProfilesDescription@");
    auto autoSwitch=RowToggle();SettingRow(games,"@WinUI.GameAutoSwitch@",autoSwitch,"@WinUI.GameAutoSwitchDescription@");
    autoSwitch.Toggled([weak=get_weak()](auto const& sender,auto&&){if(auto self=weak.get();self&&!self->m_updating&&!NativeLocalization::applying&&self->m_host)
        self->Submit(self->m_host->Core().SetGameAutoSwitch(sender.template as<ToggleSwitch>().IsOn()));});
    Divider(games);
    // A shelf of portrait covers, like a game library; the last tile adds a game.
    VariableSizedWrapGrid shelf;shelf.Orientation(Orientation::Horizontal);shelf.ItemWidth(166);shelf.ItemHeight(258);games.Children().Append(shelf);
    struct ShelfState {std::shared_ptr<const std::vector<GameProfileEntry>> list;std::string active;bool running=false,built=false;};
    auto state=std::make_shared<ShelfState>();
    v->bindings.push_back([weak=get_weak(),shelf,autoSwitch,state](auto const& s,bool){
        if(autoSwitch.IsOn()!=s.gameAutoSwitch)autoSwitch.IsOn(s.gameAutoSwitch);
        if(autoSwitch.IsEnabled()!=s.running)autoSwitch.IsEnabled(s.running);
        // The list is immutable and shared, so comparing the pointer tells whether anything changed.
        if(state->built&&state->list==s.games&&state->active==s.activeGame&&state->running==s.running)return;
        auto self=weak.get();if(!self)return;
        state->built=true;state->list=s.games;state->active=s.activeGame;state->running=s.running;
        shelf.Children().Clear();
        for(auto const& game:*s.games) {
            shelf.Children().Append(self->GameCard(game,game.id==s.activeGame,s.running));
            if(game.cover.empty()&&self->m_coverAttempts.insert(game.id).second)self->FetchCover(game);
        }
        shelf.Children().Append(self->AddGameCard(s.running));
        NativeLocalization::Invalidate();
    });
}
FrameworkElement FeaturePage::GameCard(GameProfileEntry const& game,bool active,bool enabled) {
    auto brush=[](wchar_t const* key){return Application::Current().Resources().Lookup(box_value(key)).as<Media::Brush>();};
    Grid face;
    // Shown until a cover is found, and behind it if the image cannot be read.
    Media::LinearGradientBrush gradient;gradient.StartPoint({0,0});gradient.EndPoint({1,1});
    Media::GradientStop top;top.Color(Windows::UI::Color{255,11,79,192});top.Offset(0);gradient.GradientStops().Append(top);
    Media::GradientStop bottom;bottom.Color(Windows::UI::Color{255,0,16,46});bottom.Offset(1);gradient.GradientStops().Append(bottom);
    Border fallback;fallback.Background(gradient);
    TextBlock title;title.Text(to_hstring(game.name));title.TextWrapping(TextWrapping::Wrap);title.TextAlignment(TextAlignment::Center);title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    title.Foreground(Media::SolidColorBrush(Windows::UI::Color{255,255,255,255}));title.VerticalAlignment(VerticalAlignment::Center);title.Margin(Thickness{12,12,12,12});
    fallback.Child(title);face.Children().Append(fallback);
    if(!game.cover.empty()) try {
        std::wstring location=L"file:///"+std::wstring(to_hstring(game.cover));std::replace(location.begin(),location.end(),L'\\',L'/');
        Media::Imaging::BitmapImage bitmap;bitmap.DecodePixelWidth(300);bitmap.UriSource(Windows::Foundation::Uri(location));
        Image cover;cover.Source(bitmap);cover.Stretch(Media::Stretch::UniformToFill);cover.HorizontalAlignment(HorizontalAlignment::Center);cover.VerticalAlignment(VerticalAlignment::Center);
        face.Children().Append(cover);
    } catch(...) {}
    if(active) {
        Border badge;badge.CornerRadius(CornerRadiusHelper::FromUniformRadius(4));badge.Padding(Thickness{6,2,6,3});badge.Margin(Thickness{8,8,8,8});
        badge.HorizontalAlignment(HorizontalAlignment::Left);badge.VerticalAlignment(VerticalAlignment::Bottom);badge.Background(brush(L"AccentFillColorDefaultBrush"));
        auto label=Text("@WinUI.GameActive@",11);label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());label.Foreground(brush(L"TextOnAccentFillColorPrimaryBrush"));
        badge.Child(label);face.Children().Append(badge);
    }
    Border clip;clip.CornerRadius(CornerRadiusHelper::FromUniformRadius(6));clip.Child(face);
    Controls::Button button;button.Width(150);button.Height(225);button.Padding(Thickness{0,0,0,0});button.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    button.HorizontalContentAlignment(HorizontalAlignment::Stretch);button.VerticalContentAlignment(VerticalAlignment::Stretch);
    if(active){button.BorderThickness(Thickness{2,2,2,2});button.BorderBrush(brush(L"AccentFillColorDefaultBrush"));}
    button.Content(clip);button.IsEnabled(enabled);
    Automation::AutomationProperties::SetName(button,to_hstring(game.name));ToolTipService::SetToolTip(button,box_value(to_hstring(game.name)));
    button.Click([weak=get_weak(),id=game.id](auto&&,auto&&){if(auto self=weak.get();self&&self->m_host)self->Submit(self->m_host->Core().ToggleGameProfile(id));});
    // The menu opens outside the page tree, so its text is translated here.
    MenuFlyoutItem remove;remove.Text(NativeLocalization::Text(L"@WinUI.RemoveGame@"));FontIcon bin;bin.Glyph(L"\uE74D");remove.Icon(bin);
    remove.Click([weak=get_weak(),id=game.id](auto&&,auto&&){if(auto self=weak.get();self&&self->m_host)self->Submit(self->m_host->Core().RemoveGameProfile(id));});
    MenuFlyout menu;menu.Items().Append(remove);button.ContextFlyout(menu);
    Panel card;card.Spacing(6);card.Width(150);card.HorizontalAlignment(HorizontalAlignment::Left);card.VerticalAlignment(VerticalAlignment::Top);card.Children().Append(button);
    TextBlock name;name.Text(to_hstring(game.name));name.FontSize(12);name.TextTrimming(TextTrimming::CharacterEllipsis);card.Children().Append(name);
    return card;
}
FrameworkElement FeaturePage::AddGameCard(bool enabled) {
    Panel content;content.Spacing(8);content.VerticalAlignment(VerticalAlignment::Center);content.HorizontalAlignment(HorizontalAlignment::Center);
    FontIcon plus;plus.Glyph(L"\uE710");plus.FontSize(28);content.Children().Append(plus);
    auto label=Text("@WinUI.AddGame@");label.TextAlignment(TextAlignment::Center);content.Children().Append(label);
    Controls::Button button;button.Width(150);button.Height(225);button.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));button.Content(content);button.IsEnabled(enabled);
    button.HorizontalAlignment(HorizontalAlignment::Left);button.VerticalAlignment(VerticalAlignment::Top);
    Automation::AutomationProperties::SetName(button,L"@WinUI.AddGame@");
    button.Click([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->AddGameDialog();});
    return button;
}
fire_and_forget FeaturePage::PickProfile(int operation) {
    auto lifetime=get_strong();auto host=m_host;const int controller=m_selected;const HWND window=m_window;
    if(!host||!window)co_return;
    try {
        Windows::Storage::StorageFile file{nullptr};
        if(operation==1){Windows::Storage::Pickers::FileSavePicker picker;picker.as<IInitializeWithWindow>()->Initialize(window);
            picker.SuggestedFileName(NativeLocalization::Text(L"@WinUI.ControllerProfile@"));picker.FileTypeChoices().Insert(NativeLocalization::Text(L"@WinUI.DualsenseZProfile@"),single_threaded_vector<hstring>({L".dsy"}));file=co_await picker.PickSaveFileAsync();}
        else {Windows::Storage::Pickers::FileOpenPicker picker;picker.as<IInitializeWithWindow>()->Initialize(window);picker.FileTypeFilter().Append(L".dsy");file=co_await picker.PickSingleFileAsync();}
        if(!file||host->Status()!=CoreHost::State::Running)co_return;
        auto path=to_string(file.Path());ApplicationCore::RequestId id=0;
        switch(operation){case 0:id=host->Core().LoadProfile(controller,path);break;case 1:id=host->Core().SaveProfile(controller,path);break;
        case 2:id=host->Core().SetDefaultProfile(controller,path);break;}
        Submit(id);
    }catch(hresult_error const& e){if(host->Status()==CoreHost::State::Running){m_notifications.Show(hstring(L"@WinUI.FilePickerFailed@\n")+e.message(),InfoBarSeverity::Error);}}
}
// Offers the installed Steam games that have no profile yet; any other game is added through its .exe.
fire_and_forget FeaturePage::AddGameDialog() {
    auto lifetime=get_strong();auto host=m_host;const HWND window=m_window;
    if(!host||!window)co_return;
    try {
        std::vector<InstalledGame> installed;
        const auto existing=host->Core().GetSnapshot()->games;
        for(auto& game:GameLibrary::InstalledSteamGames())
            if(std::none_of(existing->begin(),existing->end(),[&](GameProfileEntry const& added){return added.match==game.folder||(added.steamAppId>0&&added.steamAppId==game.steamAppId);}))installed.push_back(std::move(game));
        // A dialog is outside the page tree, so every string is translated here.
        ContentDialog dialog;dialog.XamlRoot(XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
        dialog.Title(box_value(NativeLocalization::Text(L"@WinUI.AddGame@")));
        Panel body;body.Spacing(12);body.MinWidth(360);
        TextBlock description;description.Text(NativeLocalization::Text(installed.empty()?L"@WinUI.NoSteamGames@":L"@WinUI.AddGameDescription@"));description.TextWrapping(TextWrapping::Wrap);body.Children().Append(description);
        ListView list;list.SelectionMode(ListViewSelectionMode::Single);list.MaxHeight(320);
        for(auto const& game:installed)list.Items().Append(box_value(to_hstring(game.name)));
        if(!installed.empty())body.Children().Append(list);
        dialog.Content(body);
        dialog.PrimaryButtonText(NativeLocalization::Text(L"@WinUI.AddSelectedGame@"));dialog.SecondaryButtonText(NativeLocalization::Text(L"@WinUI.BrowseForExe@"));
        dialog.CloseButtonText(NativeLocalization::Text(L"@WinUI.CancelButton@"));dialog.DefaultButton(ContentDialogButton::Primary);dialog.IsPrimaryButtonEnabled(false);
        list.SelectionChanged([dialog,list](auto&&,auto&&){dialog.IsPrimaryButtonEnabled(list.SelectedIndex()>=0);});
        const auto result=co_await dialog.ShowAsync();
        if(host->Status()!=CoreHost::State::Running)co_return;
        if(result==ContentDialogResult::Primary) {
            const int index=list.SelectedIndex();
            if(index>=0&&index<int(installed.size()))Submit(host->Core().AddGameProfile(installed[index].name,installed[index].folder,installed[index].steamAppId));
        } else if(result==ContentDialogResult::Secondary) {
            Windows::Storage::Pickers::FileOpenPicker picker;picker.as<IInitializeWithWindow>()->Initialize(window);picker.FileTypeFilter().Append(L".exe");
            const auto file=co_await picker.PickSingleFileAsync();
            if(!file||host->Status()!=CoreHost::State::Running)co_return;
            const auto path=to_string(file.Path());
            Submit(host->Core().AddGameProfile(GameLibrary::ExecutableName(path),path,0));
        }
    }catch(hresult_error const& e){if(host->Status()==CoreHost::State::Running){m_notifications.Show(hstring(L"@WinUI.FilePickerFailed@\n")+e.message(),InfoBarSeverity::Error);}}
}
// Downloads the portrait cover Steam publishes for a game. A game that is not from the Steam
// library is first looked up by name in the Steam store. Failures are silent: the tile keeps its name.
fire_and_forget FeaturePage::FetchCover(GameProfileEntry game) {
    const auto host=m_host;
    if(!host)co_return;
    try {
        co_await resume_background();
        Windows::Web::Http::HttpClient client;
        client.DefaultRequestHeaders().UserAgent().TryParseAdd(L"DualsenseZ");
        int appId=game.steamAppId;
        if(appId<=0) {
            const auto found=co_await client.GetStringAsync(Windows::Foundation::Uri(L"https://store.steampowered.com/api/storesearch/?l=english&cc=US&term="+Windows::Foundation::Uri::EscapeComponent(to_hstring(game.name))));
            for(auto const& item:Windows::Data::Json::JsonObject::Parse(found).GetNamedArray(L"items",Windows::Data::Json::JsonArray())) {
                const auto entry=item.GetObject();
                if(entry.GetNamedString(L"type",L"")==L"app"){appId=int(entry.GetNamedNumber(L"id",0));break;}
            }
            if(appId<=0)co_return;
        }
        const auto image=co_await client.GetBufferAsync(Windows::Foundation::Uri(L"https://cdn.cloudflare.steamstatic.com/steam/apps/"+to_hstring(appId)+L"/library_600x900.jpg"));
        // Only keep what is plausibly a JPEG cover.
        if(image.Length()<1024||image.Length()>8*1024*1024||image.data()[0]!=0xFF||image.data()[1]!=0xD8)co_return;
        const auto file=std::filesystem::path(std::wstring(to_hstring(GameLibrary::CoversFolder())))/(std::to_wstring(appId)+L".jpg");
        {std::ofstream output(file,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(image.data()),image.Length());if(!output)co_return;}
        host->Core().SetGameCover(game.id,to_string(file.wstring()));
    } catch(...) {}
}
}
