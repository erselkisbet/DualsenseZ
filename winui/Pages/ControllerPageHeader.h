#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <limits>

namespace ControllerPageHeader {
// One layout and selector sizing policy for all controller-related page headers.
inline void Attach(winrt::Microsoft::UI::Xaml::Controls::Grid const& header) {
    using namespace winrt;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    auto selector=header.Children().GetAt(1).as<ComboBox>();
    selector.Header(box_value(L"@WinUI.SelectedController@"));selector.FontSize(14);
    selector.VerticalAlignment(VerticalAlignment::Top);
    Automation::AutomationProperties::SetName(selector,L"@WinUI.SelectedController@");
    if(selector.Items().Size()==0)for(auto label:{L"@WinUI.Controller1@",L"@WinUI.Controller2@",L"@WinUI.Controller3@",L"@WinUI.Controller4@"}) {
        ComboBoxItem item;item.Content(box_value(hstring(label)));selector.Items().Append(item);
    }
    for(int i=0;i<2;++i){header.ColumnDefinitions().Append(ColumnDefinition());RowDefinition row;row.Height(GridLengthHelper::Auto());header.RowDefinitions().Append(row);}
    auto arrange=[weak=make_weak(header)](double width){if(auto layout=weak.get()) {
        const bool narrow=width<700;
        auto labels=layout.Children().GetAt(0).as<FrameworkElement>();auto choice=layout.Children().GetAt(1).as<ComboBox>();
        layout.ColumnSpacing(narrow?0:16);layout.RowSpacing(narrow?12:0);
        layout.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromValueAndType(1,GridUnitType::Star));
        layout.ColumnDefinitions().GetAt(1).Width(narrow?GridLengthHelper::FromValueAndType(0,GridUnitType::Pixel):GridLengthHelper::Auto());
        Grid::SetColumnSpan(labels,narrow?2:1);
        Grid::SetColumn(choice,narrow?0:1);Grid::SetRow(choice,narrow?1:0);Grid::SetColumnSpan(choice,narrow?2:1);
        choice.Width(narrow?std::numeric_limits<double>::quiet_NaN():260);
        choice.MaxWidth(std::numeric_limits<double>::infinity());
        choice.HorizontalAlignment(narrow?HorizontalAlignment::Stretch:HorizontalAlignment::Right);
    }};
    header.SizeChanged([arrange](auto const&,SizeChangedEventArgs const& args){arrange(args.NewSize().Width);});
    arrange(header.ActualWidth());
}
}
