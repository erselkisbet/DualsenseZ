#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.xaml.g.hpp"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "Pages/OverviewPage.xaml.h"
#include "Pages/SettingsPage.xaml.h"
#include "Pages/ControllerPage.xaml.h"
#include "Pages/FeaturePage.xaml.h"
#include <microsoft.ui.xaml.window.h>
#include <dwmapi.h>
#include <shellapi.h>
#include "resource.h"
#include <algorithm>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace winrt::DualsenseZNative::implementation {
MainWindow::MainWindow() {
    InitializeComponent();
    // Preserve the standard system caption and native hit testing.
    ExtendsContentIntoTitleBar(false);
    check_hresult(this->try_as<IWindowNative>()->get_WindowHandle(&m_hwnd));
    SetWindowSubclass(m_hwnd,WindowProc,1,reinterpret_cast<DWORD_PTR>(this));
    UpdateWindowIcons(GetDpiForWindow(m_hwnd));
    AppWindow().Resize({1200,850});
    if(Microsoft::UI::Composition::SystemBackdrops::MicaController::IsSupported())
        SystemBackdrop(Media::MicaBackdrop());
    Root().ActualThemeChanged([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->UpdateTitleTheme();});
    Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->CloseHost();});
    UpdateTitleTheme();
    m_notificationTimer=DispatcherQueue().CreateTimer();m_notificationTimer.IsRepeating(false);
    m_notificationTimer.Tick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->NotificationPopup().IsOpen(false);});
    NotificationPopup().RegisterPropertyChangedCallback(InfoBar::IsOpenProperty(),[weak=get_weak()](auto const&,auto const&){if(auto self=weak.get()){
        self->NotificationPopup().Visibility(self->NotificationPopup().IsOpen()?Visibility::Visible:Visibility::Collapsed);
        if(!self->NotificationPopup().IsOpen())self->m_notificationTimer.Stop();
    }});
    Root().SizeChanged([weak=get_weak()](auto const&,SizeChangedEventArgs const& args){if(auto self=weak.get())self->NotificationPopup().MaxWidth(std::max(0.0,std::min<double>(560.0,args.NewSize().Width-48)));});
    auto fade=Root().Resources().Lookup(box_value(L"NotificationFadeIn")).as<Media::Animation::Storyboard>();
    Media::Animation::Storyboard::SetTarget(fade.Children().GetAt(0),NotificationPopup());
}
MainWindow::~MainWindow() {CloseHost();if(m_appIcon)DestroyIcon(m_appIcon);if(m_titleIcon)DestroyIcon(m_titleIcon);}
LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
    auto self=reinterpret_cast<MainWindow*>(data);
    if(self&&!self->m_closed) {
        if(message==WM_DPICHANGED)self->UpdateWindowIcons(HIWORD(w));
        if(message==WM_SHOWWINDOW)self->VisibilityChanged(w!=0);
        if(message==WM_TIMER&&w==42){if(self->m_host&&(self->m_host->TakeBringToFront()||self->m_host->Status()==CoreHost::State::Failed))self->ShowFromTray();return 0;}
        if(message==WM_SYSCOMMAND&&(w&0xfff0)==SC_MINIMIZE&&self->m_hideOnMinimize&&self->m_trayAdded){ShowWindow(hwnd,SW_HIDE);return 0;}
        // Close-to-tray: only while the runtime is healthy and the tray icon exists; Exit in the tray menu always quits.
        if(message==WM_CLOSE&&self->m_hideOnClose&&!self->m_exitRequested&&self->m_trayAdded&&self->m_host&&self->m_host->Status()==CoreHost::State::Running){ShowWindow(hwnd,SW_HIDE);return 0;}
        if(message==WM_APP+42) {
            if(l==WM_LBUTTONDBLCLK)self->ShowFromTray();
            if(l==WM_RBUTTONUP){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,NativeLocalization::Text(L"@WinUI.ShowDualsenseZ@").c_str());AppendMenuW(menu,MF_STRING,2,NativeLocalization::Text(L"@WinUI.HideToTray@").c_str());AppendMenuW(menu,MF_STRING,3,NativeLocalization::Text(L"@WinUI.Exit@").c_str());POINT p;GetCursorPos(&p);SetForegroundWindow(hwnd);int action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(menu);if(action==1)self->ShowFromTray();else if(action==2)ShowWindow(hwnd,SW_HIDE);else if(action==3){self->m_exitRequested=true;PostMessageW(hwnd,WM_CLOSE,0,0);}}
            return 0;
        }
        if(message==RegisterWindowMessageW(L"TaskbarCreated")){self->m_trayAdded=false;self->AddTray();if(!self->m_trayAdded)self->ShowFromTray();}
    }
    if(message==WM_GETMINMAXINFO) {
        auto info=reinterpret_cast<MINMAXINFO*>(l);const auto dpi=GetDpiForWindow(hwnd);
        info->ptMinTrackSize={MulDiv(760,dpi,96),MulDiv(560,dpi,96)};return 0;
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,WindowProc,1);
    return DefSubclassProc(hwnd,message,w,l);
}
void MainWindow::UpdateWindowIcons(UINT dpi) {
    const auto module=GetModuleHandleW(nullptr);
    auto replace=[&](HICON& current,int resource,WPARAM role,int width,int height){
        auto next=static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(resource),IMAGE_ICON,width,height,LR_DEFAULTCOLOR));
        if(!next)return;
        SendMessageW(m_hwnd,WM_SETICON,role,reinterpret_cast<LPARAM>(next));
        if(current)DestroyIcon(current);current=next;
    };
    // Windows keeps these roles separate: caption logo versus app/task-switcher artwork.
    replace(m_appIcon,IDI_DUALSENSEZ,ICON_BIG,GetSystemMetricsForDpi(SM_CXICON,dpi),GetSystemMetricsForDpi(SM_CYICON,dpi));
    // The caption logo is white for a dark title bar and blue for a light one.
    const bool dark=Root().ActualTheme()==ElementTheme::Dark;
    replace(m_titleIcon,dark?IDI_DUALSENSEZ_TITLEBAR:IDI_DUALSENSEZ_TITLEBAR_ON_LIGHT,ICON_SMALL,GetSystemMetricsForDpi(SM_CXSMICON,dpi),GetSystemMetricsForDpi(SM_CYSMICON,dpi));
}
void MainWindow::UpdateTitleTheme() {
    BOOL dark=Root().ActualTheme()==ElementTheme::Dark;
    DwmSetWindowAttribute(m_hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    UpdateWindowIcons(GetDpiForWindow(m_hwnd));
}
void MainWindow::AddTray() {
    NOTIFYICONDATAW icon{};icon.cbSize=sizeof(icon);icon.hWnd=m_hwnd;icon.uID=1;icon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;icon.uCallbackMessage=WM_APP+42;
    icon.hIcon=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDI_DUALSENSEZ),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));wcscpy_s(icon.szTip,L"DualsenseZ");m_trayAdded=Shell_NotifyIconW(NIM_ADD,&icon)!=FALSE;
}
void MainWindow::ShowFromTray() {
    if(m_closed)return;
    ShowWindow(m_hwnd,SW_RESTORE);SetForegroundWindow(m_hwnd);Activate();
    VisibilityChanged(true);
}
void MainWindow::VisibilityChanged(bool visible) {
    if(m_closed||!m_timer)return;
    if(visible){KillTimer(m_hwnd,42);m_timer.Start();}
    else {
        m_timer.Stop();
        if(auto feature=ContentFrame().Content().try_as<DualsenseZNative::FeaturePage>())get_self<FeaturePage>(feature)->Suspend();
        // Only host wakeups are polled while hidden. No XAML/page refresh work.
        SetTimer(m_hwnd,42,250,nullptr);
    }
}
void MainWindow::Attach(std::shared_ptr<CoreHost> host) {
    m_host=std::move(host);AddTray();
    m_timer=DispatcherQueue().CreateTimer();
    m_timer.Interval(std::chrono::milliseconds(33));
    m_timerToken=m_timer.Tick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->Refresh();});
    m_timer.Start();
    // Initial navigation can apply the tray-start preference and pause this timer.
    Navigation().SelectedItem(Navigation().MenuItems().GetAt(0));
}
void MainWindow::CloseHost() {
    if(m_closed)return;m_closed=true;
    if(m_timer){m_timer.Stop();m_timer.Tick(m_timerToken);m_timer=nullptr;}
    if(m_hwnd)KillTimer(m_hwnd,42);
    if(m_trayAdded){NOTIFYICONDATAW icon{};icon.cbSize=sizeof(icon);icon.hWnd=m_hwnd;icon.uID=1;Shell_NotifyIconW(NIM_DELETE,&icon);m_trayAdded=false;}
    SystemBackdrop(nullptr);
    if(m_notificationTimer)m_notificationTimer.Stop();m_notifications.clear();
    if(m_host)m_host->StopAndJoin();
    if(m_hwnd&&IsWindow(m_hwnd))RemoveWindowSubclass(m_hwnd,WindowProc,1);
}
void MainWindow::Navigate(NavigationView const&,NavigationViewSelectionChangedEventArgs const& args) {
    if(!m_host)return;
    hstring name=args.IsSettingsSelected()?L"Settings":unbox_value<hstring>(args.SelectedItemContainer().Tag());
    if(name==L"Overview") {
        ContentFrame().Navigate(xaml_typename<DualsenseZNative::OverviewPage>());
        get_self<OverviewPage>(ContentFrame().Content().as<DualsenseZNative::OverviewPage>())->Attach(m_host,[weak=get_weak()](hstring const& destination) {
            if(auto self=weak.get())for(auto item:self->Navigation().MenuItems()) {
                if(auto entry=item.try_as<NavigationViewItem>();entry&&unbox_value<hstring>(entry.Tag())==destination) {
                    self->Navigation().SelectedItem(entry);break;
                }
            }
        },Notifications());
    } else if(name==L"Controller") {
        ContentFrame().Navigate(xaml_typename<DualsenseZNative::ControllerPage>());
        get_self<ControllerPage>(ContentFrame().Content().as<DualsenseZNative::ControllerPage>())->Attach(m_host,m_hwnd,[weak=get_weak()] {
            if(auto self=weak.get())self->Navigation().SelectedItem(self->Navigation().SettingsItem());
        },Notifications());
    } else if(name==L"Settings") {
        ContentFrame().Navigate(xaml_typename<DualsenseZNative::SettingsPage>());
        m_settingsPage=ContentFrame().Content().as<Microsoft::UI::Xaml::Controls::Page>();
        get_self<SettingsPage>(ContentFrame().Content().as<DualsenseZNative::SettingsPage>())->Attach(m_host,Notifications());
    } else {
        ContentFrame().Navigate(xaml_typename<DualsenseZNative::FeaturePage>(),box_value(name));
        get_self<FeaturePage>(ContentFrame().Content().as<DualsenseZNative::FeaturePage>())->Attach(m_host,m_hwnd,Notifications());
    }
    // NavigationView maintains selection, not an ever-growing back stack.
    ContentFrame().BackStack().Clear();
    NativeLocalization::Invalidate();
    Refresh();
}
NotificationClient MainWindow::Notifications() {
    return {
        [weak=get_weak()](auto const& message,auto severity){if(auto self=weak.get())self->ShowNotification(message,severity);},
        [weak=get_weak()](auto const& ids,auto const& success,auto severity){if(auto self=weak.get())self->TrackNotifications(ids,success,severity);}
    };
}
void MainWindow::ShowNotification(hstring const& message,InfoBarSeverity severity) {
    if(m_closed)return;
    m_notificationTimer.Stop();
    auto popup=NotificationPopup();popup.Severity(severity);popup.Title(message);popup.Message(L"");
    m_translator.Walk(popup);popup.IsOpen(true);
    auto fade=Root().Resources().Lookup(box_value(L"NotificationFadeIn")).as<Media::Animation::Storyboard>();fade.Stop();fade.Begin();
    m_notificationTimer.Interval(std::chrono::seconds(severity==InfoBarSeverity::Error?6:severity==InfoBarSeverity::Warning?4:3));m_notificationTimer.Start();
}
void MainWindow::TrackNotifications(std::vector<ApplicationCore::RequestId> const& ids,hstring const& success,InfoBarSeverity severity) {
    auto pending=std::make_shared<PendingNotification>();pending->success=success;pending->severity=severity;
    for(auto id:ids){if(id){++pending->remaining;m_notifications[id]=pending;}else pending->failed=true;}
    if(pending->failed)ShowNotification(L"@WinUI.CheckTheValuesAndRuntimeConnection@",InfoBarSeverity::Error);
}
void MainWindow::NotifyCompleted(ApplicationCore::CommandResult const& result) {
    auto found=m_notifications.find(result.id);std::shared_ptr<PendingNotification> pending;
    if(found!=m_notifications.end()){pending=found->second;m_notifications.erase(found);--pending->remaining;}
    const bool succeeded=result.status==ApplicationCore::CommandStatus::Succeeded;
    if(!succeeded){
        if(!pending||!pending->failed)ShowNotification(result.messageKey.empty()?hstring(L"@WinUI.FailedToApplyChanges@"):to_hstring(result.messageKey),InfoBarSeverity::Error);
        if(pending)pending->failed=true;
    }else if(pending&&!pending->failed&&!pending->remaining&&!pending->success.empty()){
        // Specific local-profile results are localized by the existing dictionary.
        const bool specific=pending->severity==InfoBarSeverity::Success&&!result.messageKey.empty();
        ShowNotification(specific?to_hstring(result.messageKey):pending->success,pending->severity);
    }
}
void MainWindow::Refresh() {
    if(!m_host||m_closed)return;
    if(m_startBehaviorApplied&&!IsWindowVisible(m_hwnd))return;
    const auto state=m_host->Status();
    if(state==CoreHost::State::Forwarded){Close();return;}
    if(state!=m_lastState) {
        m_lastState=state;NativeLocalization::Invalidate();
        if(state==CoreHost::State::Running)StatusBar().IsOpen(false);
        if(state==CoreHost::State::Failed) {
            ShowFromTray();
            StatusBar().Severity(InfoBarSeverity::Error);StatusBar().Title(L"@WinUI.RuntimeUnavailable@");
            StatusBar().Message(to_hstring(m_host->Error()));StatusBar().IsOpen(true);
        }
    }
    if(m_host->TakeBringToFront())ShowFromTray();
    auto content=ContentFrame().Content();
    auto results=m_host->Core().DrainResults();
    auto snapshot=m_host->Core().GetSnapshot();
    m_translator.Set(*snapshot);
    for(const auto& result:results) {
        if(m_settingsPage)get_self<SettingsPage>(m_settingsPage.as<DualsenseZNative::SettingsPage>())->Completed(result);
        NotifyCompleted(result);
    }
    Root().FlowDirection(snapshot->preferences.language=="ar"?FlowDirection::RightToLeft:FlowDirection::LeftToRight);
    m_hideOnMinimize=snapshot->preferences.hideToTrayOnMinimize;m_hideOnClose=snapshot->preferences.hideToTrayOnClose;
    if(snapshot->running&&!m_startBehaviorApplied){m_startBehaviorApplied=true;if(snapshot->preferences.hideToTrayOnStart&&m_trayAdded){ShowWindow(m_hwnd,SW_HIDE);VisibilityChanged(false);}else ShowFromTray();}
    if(m_startBehaviorApplied&&!IsWindowVisible(m_hwnd))return;
    if(auto feature=content.try_as<DualsenseZNative::FeaturePage>())get_self<FeaturePage>(feature)->Update(*snapshot);
    if(auto controller=content.try_as<DualsenseZNative::ControllerPage>())get_self<ControllerPage>(controller)->Update(*snapshot);
    if(auto overview=content.try_as<DualsenseZNative::OverviewPage>())get_self<OverviewPage>(overview)->Update(*snapshot);
    if(auto settings=content.try_as<DualsenseZNative::SettingsPage>())get_self<SettingsPage>(settings)->Update(*snapshot);
    m_translator.Set(*snapshot);if(m_translator.Due())m_translator.Walk(Root());
}
}


