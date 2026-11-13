#pragma once
#include "core/applicationCore.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
// UI owns this wrapper; only the worker invokes the core lifecycle. No WinUI
// objects/callbacks cross into the runtime thread. StopAndJoin is idempotent.
class CoreHost final {
public:
    enum class State { Starting, Running, Forwarded, Failed, Stopped };
    explicit CoreHost(std::string profilePath);
    ~CoreHost();
    void StopAndJoin();
    ApplicationCore& Core() {return m_core;}
    State Status() const {return m_state.load();}
    bool TakeBringToFront() {return m_bringToFront.exchange(false);}
    std::string Error() const;
    std::uint64_t TickCount() const {return m_ticks.load();}
private:
    ApplicationCore m_core;
    std::atomic<State> m_state{State::Starting};
    std::atomic<bool> m_bringToFront{false};
    std::atomic<std::uint64_t> m_ticks{0};
    mutable std::mutex m_mutex;
    std::condition_variable_any m_wake;
    std::string m_error;
    std::jthread m_thread; // last member: joined before the core can be destroyed
};
