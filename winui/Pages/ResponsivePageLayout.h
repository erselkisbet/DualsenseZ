#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace ResponsivePageLayout {
inline void Attach(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                   double leftWeight=1, double rightWeight=1, double breakpoint=700) {
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    auto resize=[weak=winrt::make_weak(grid),leftWeight,rightWeight,breakpoint](double width) {
        auto layout=weak.get();
        if(!layout||layout.Children().Size()<2||layout.ColumnDefinitions().Size()<2)return;
        const bool narrow=width<breakpoint;
        layout.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromValueAndType(narrow?1:leftWeight,GridUnitType::Star));
        layout.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromValueAndType(narrow?0:rightWeight,narrow?GridUnitType::Pixel:GridUnitType::Star));
        auto left=layout.Children().GetAt(0).as<FrameworkElement>(),right=layout.Children().GetAt(1).as<FrameworkElement>();
        Grid::SetColumn(left,0);Grid::SetRow(left,0);Grid::SetColumnSpan(left,narrow?2:1);
        Grid::SetColumn(right,narrow?0:1);Grid::SetRow(right,narrow?1:0);Grid::SetColumnSpan(right,narrow?2:1);
    };
    grid.SizeChanged([resize](auto const&, SizeChangedEventArgs const& args){resize(args.NewSize().Width);});
    resize(grid.ActualWidth());
}
}
