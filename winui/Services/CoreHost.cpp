#include "pch.h"
#include "CoreHost.h"
#include <fstream>
CoreHost::CoreHost(std::string path) : m_thread([this,path=std::move(path)](std::stop_token stop) {
    // Audio/COM work is initialized on the owner thread, independently of XAML's STA.
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try {
        if(m_core.Start(path)==ApplicationCore::StartResult::ForwardedToExistingInstance) {
            m_state=State::Forwarded;
        } else {
            m_state=State::Running;
            auto deadline=std::chrono::steady_clock::now();
            while(!stop.stop_requested()) {
                if(m_core.Tick().bringToFront) m_bringToFront=true;
                ++m_ticks;
                deadline+=std::chrono::milliseconds(10);
                if(deadline<std::chrono::steady_clock::now()) deadline=std::chrono::steady_clock::now();
                std::unique_lock lock(m_mutex);
                m_wake.wait_until(lock,stop,deadline,[]{return false;});
            }
        }
    } catch(const std::exception& e) {
        {std::lock_guard lock(m_mutex);m_error=e.what();}
        m_state=State::Failed;
    } catch(...) {
        {std::lock_guard lock(m_mutex);m_error="ApplicationCore startup/runtime failed.";}
        m_state=State::Failed;
    }
    m_core.Stop();
    if(m_state==State::Running) m_state=State::Stopped;
    // Local diagnostic evidence of lifecycle, no device or user data.
    std::ofstream("winui-runtime.log",std::ios::app)<<"Stopped; ticks="<<m_ticks.load()<<"; state="<<int(m_state.load())<<'\n';
    if(SUCCEEDED(com)) CoUninitialize();
}) {}
CoreHost::~CoreHost() {StopAndJoin();}
void CoreHost::StopAndJoin() {
    if(m_thread.joinable()) {m_thread.request_stop();m_wake.notify_all();m_thread.join();}
}
std::string CoreHost::Error() const {std::lock_guard lock(m_mutex);return m_error;}
