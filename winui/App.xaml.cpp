#include "pch.h"
#include "App.xaml.h"
#include "App.xaml.g.hpp"
#include "MainWindow.xaml.h"
#include <filesystem>
#include <shellapi.h>
using namespace winrt;
namespace winrt::DualsenseZNative::implementation {
App::App() {InitializeComponent();}
App::~App() {if(m_host)m_host->StopAndJoin();}
void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
    std::filesystem::current_path(std::filesystem::path(exe).parent_path());
    int count=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&count);
    std::string profile=count>1?to_string(argv[1]):"";LocalFree(argv);
    m_host=std::make_shared<CoreHost>(profile);
    auto window=make<MainWindow>();
    // Register before Attach: a forwarded second instance closes its window during Attach,
    // and a window still held here at XAML shutdown is released too late and crashes.
    m_window=window;
    m_window.Closed([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->m_window=nullptr;});
    get_self<MainWindow>(window)->Attach(m_host);
    // MainWindow activates only after the startup preference snapshot arrives.
}
}
