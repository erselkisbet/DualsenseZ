#pragma once
#include "scePadSettings.hpp"
#include <array>
#include <memory>
#include <atomic>
#include <mutex>

// Runtime publishes configuration; workers never borrow mutable profile storage.
struct WorkerConfiguration {
    std::array<s_scePadSettings,4> controllers;
    unsigned selected=0;
};
class WorkerExchange {
    std::shared_ptr<const WorkerConfiguration> configuration=std::make_shared<WorkerConfiguration>();
    struct Feedback {
        bool rumbleDirty=false, lightDirty=false;
        s_ScePadVibrationParam rumble{};
        s_SceLightBar light{};
        unsigned gyroToggles=0;
    };
    std::mutex feedbackMutex;
    std::array<Feedback,4> feedback{};
public:
    void Publish(const s_scePadSettings* settings,unsigned selected) {
        auto next=std::make_shared<WorkerConfiguration>();
        for(int i=0;i<4;++i) next->controllers[i]=settings[i];
        next->selected=selected;
        std::shared_ptr<const WorkerConfiguration> immutable=std::move(next);
        std::atomic_store(&configuration,std::move(immutable));
    }
    auto Read() const {return std::atomic_load(&configuration);}
    void Rumble(unsigned i,s_ScePadVibrationParam value) {
        std::lock_guard lock(feedbackMutex);feedback[i].rumble=value;feedback[i].rumbleDirty=true;
    }
    void Light(unsigned i,s_SceLightBar value) {
        std::lock_guard lock(feedbackMutex);feedback[i].light=value;feedback[i].lightDirty=true;
    }
    void ToggleGyro(unsigned i) {std::lock_guard lock(feedbackMutex);++feedback[i].gyroToggles;}
    void Apply(s_scePadSettings* settings) {
        std::array<Feedback,4> batch;
        {std::lock_guard lock(feedbackMutex);batch=feedback;feedback={};}
        for(int i=0;i<4;++i) {
            auto& f=batch[i];auto& s=settings[i];
            if(f.rumbleDirty)s.rumbleFromEmulatedController=f.rumble;
            if(f.lightDirty){s.lightbarFromEmulatedController=f.light;s.gameLightbarAvailable=true;}
            if(f.gyroToggles%2)s.gyroToRightStickPermanent=!s.gyroToRightStickPermanent;
        }
    }
};
