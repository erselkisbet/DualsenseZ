#pragma once
#include "core/applicationSnapshot.hpp"
#include "core/uiText.hpp"
#include <map>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
namespace NativeLocalization {
// UI-thread-only immutable dictionary; no filesystem work on the UI thread.
inline std::shared_ptr<const UiText::Dictionary> current;
inline bool applying=false;
inline winrt::hstring Text(std::wstring_view value) {
    auto text=winrt::to_string(value);
    return winrt::to_hstring(current?UiText::Resolve(*current,text):text);
}
inline std::string Value(std::string const& value) {return current?UiText::Resolve(*current,value):value;}
// Set whenever elements carrying untranslated resource keys are created, so the next refresh re-walks the tree.
inline bool pending=true;
inline void Invalidate() {pending=true;}
// Per-frame bindings translate first and write only real changes; the walker never has to undo a raw value.
template<class T> void SetText(T const& target,winrt::hstring const& raw) {auto value=Text(raw);if(target.Text()!=value)target.Text(value);}
template<class T> void SetText(T const& target,std::string const& raw) {auto value=winrt::to_hstring(Value(raw));if(target.Text()!=value)target.Text(value);}
inline void SetInfo(winrt::Microsoft::UI::Xaml::Controls::InfoBar const& bar,winrt::hstring const& title,winrt::hstring const& message) {
    auto t=Text(title),m=Text(message);if(bar.Title()!=t)bar.Title(t);if(bar.Message()!=m)bar.Message(m);
}
inline bool IsString(winrt::Windows::Foundation::IInspectable const& value,winrt::hstring const& text) {
    auto boxed=value.try_as<winrt::Windows::Foundation::IPropertyValue>();
    return boxed&&boxed.Type()==winrt::Windows::Foundation::PropertyType::String&&boxed.GetString()==text;
}
struct Translator {
    void Set(ApplicationSnapshot const& snapshot) {current=snapshot.uiText;}
    // The full walk is only needed after a language change, new elements, or as a slow safety net.
    bool Due() {
        if(!pending&&current==m_walked&&++m_idle<15)return false;
        pending=false;m_walked=current;m_idle=0;return true;
    }
    winrt::hstring Translate(winrt::Microsoft::UI::Xaml::DependencyObject const& owner,winrt::hstring const& value,std::wstring const& slot) const {
        using namespace winrt;using namespace Microsoft::UI::Xaml;
        // Separate source/last values per property preserve cached pages and dynamic text.
        static std::map<std::wstring,std::pair<DependencyProperty,DependencyProperty>> properties;
        auto found=properties.find(slot);
        if(found==properties.end())found=properties.emplace(slot,std::make_pair(
            DependencyProperty::RegisterAttached(hstring(L"Source"+slot),xaml_typename<hstring>(),xaml_typename<FrameworkElement>(),PropertyMetadata(box_value(L""))),
            DependencyProperty::RegisterAttached(hstring(L"Last"+slot),xaml_typename<hstring>(),xaml_typename<FrameworkElement>(),PropertyMetadata(box_value(L""))))).first;
        auto [source,last]=found->second;auto original=unbox_value<hstring>(owner.GetValue(source));
        if(original.empty()||value!=unbox_value<hstring>(owner.GetValue(last))){original=value;owner.SetValue(source,box_value(original));}
        auto translated=Text(original);owner.SetValue(last,box_value(translated));return translated;
    }
    template<class T> void Header(T const& control) const {
        using namespace winrt;auto value=control.Header().template try_as<Windows::Foundation::IPropertyValue>();
        if(value&&value.Type()==Windows::Foundation::PropertyType::String){auto old=value.GetString(),translated=Translate(control,old,L"Header");if(old!=translated)control.Header(box_value(translated));}
    }
    void Walk(winrt::Microsoft::UI::Xaml::DependencyObject const& node) const {
        using namespace winrt;using namespace Microsoft::UI::Xaml;using namespace Controls;
        if(!node)return;
        struct Guard {bool previous=applying;Guard(){applying=true;}~Guard(){applying=previous;}} guard;
        auto name=Automation::AutomationProperties::GetName(node);
        if(!name.empty()){auto value=Translate(node,name,L"AutomationName");if(name!=value)Automation::AutomationProperties::SetName(node,value);}
        if(auto text=node.try_as<TextBlock>()){auto old=text.Text(),value=Translate(text,old,L"Text");if(old!=value)text.Text(value);return;}
        if(auto info=node.try_as<InfoBar>()){
            auto oldTitle=info.Title(),title=Translate(info,oldTitle,L"Title");if(oldTitle!=title)info.Title(title);
            auto oldMessage=info.Message(),message=Translate(info,oldMessage,L"Message");if(oldMessage!=message)info.Message(message);
            if(auto child=info.Content().try_as<DependencyObject>())Walk(child);
        }
        if(auto box=node.try_as<ComboBox>()){
            Header(box);auto selected=box.SelectedItem().try_as<ComboBoxItem>();
            auto before=selected?selected.Content():nullptr;
            for(auto item:box.Items())if(auto child=item.try_as<DependencyObject>())Walk(child);
            // WinUI caches the selection-box string; refresh it under the UI-only
            // localization guard, so SelectionChanged cannot submit commands.
            if(selected&&before!=selected.Content()){auto index=box.SelectedIndex();box.SelectedIndex(-1);box.SelectedIndex(index);}
        }
        else if(auto box=node.try_as<NumberBox>())Header(box);
        else if(auto box=node.try_as<ToggleSwitch>()){Header(box);auto on=Text(L"@WinUI.On@"),off=Text(L"@WinUI.Off@");if(!IsString(box.OnContent(),on))box.OnContent(box_value(on));if(!IsString(box.OffContent(),off))box.OffContent(box_value(off));}
        else if(auto box=node.try_as<TextBox>()){Header(box);auto old=box.PlaceholderText(),value=Translate(box,old,L"Placeholder");if(old!=value)box.PlaceholderText(value);}
        if(auto nav=node.try_as<NavigationView>()) {
            for(auto item:nav.MenuItems())if(auto d=item.try_as<DependencyObject>())Walk(d);
            for(auto item:nav.FooterMenuItems())if(auto d=item.try_as<DependencyObject>())Walk(d);
            if(auto settings=nav.SettingsItem().try_as<NavigationViewItem>()){auto label=Text(L"@Settings@");if(!IsString(settings.Content(),label))settings.Content(box_value(label));}
        }
        if(auto page=node.try_as<Page>()){if(auto child=page.Content().try_as<DependencyObject>())Walk(child);return;}
        if(auto scroll=node.try_as<ScrollViewer>()){if(auto child=scroll.Content().try_as<DependencyObject>())Walk(child);return;}
        if(auto frame=node.try_as<Frame>()){if(auto child=frame.Content().try_as<DependencyObject>())Walk(child);return;}
        if(auto content=node.try_as<ContentControl>()) {
            auto value=content.Content();if(auto text=value.try_as<Windows::Foundation::IPropertyValue>();text&&text.Type()==Windows::Foundation::PropertyType::String){auto old=text.GetString(),translated=Translate(content,old,L"Content");if(old!=translated)content.Content(box_value(translated));}
            else if(auto child=value.try_as<DependencyObject>())Walk(child);
        }else if(auto panel=node.try_as<Panel>()){for(auto child:panel.Children())Walk(child);}
        else if(auto border=node.try_as<Border>())Walk(border.Child());
    }
private:
    std::shared_ptr<const UiText::Dictionary> m_walked;
    int m_idle=0;
};
}
