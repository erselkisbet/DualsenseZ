#include "core/workerExchange.hpp"
#include <thread>
#include <stdexcept>

void TestWorkerExchange() {
    WorkerExchange exchange;
    s_scePadSettings settings[4]{};
    const auto original=exchange.Read();
    std::atomic<bool> done=false, valid=true;
    std::thread reader([&] {
        while(!done) {
            auto view=exchange.Read();
            for(int i=1;i<4;++i)
                if(view->controllers[i].leftStickDeadzone!=view->controllers[0].leftStickDeadzone)
                    valid=false;
        }
    });
    std::thread feedback([&] {
        for(int n=0;n<10001;++n) exchange.ToggleGyro(0);
    });
    const bool initialGyro=settings[0].gyroToRightStickPermanent;
    for(int n=0;n<10000;++n) {
        for(auto& s:settings)s.leftStickDeadzone=n%128;
        exchange.Apply(settings);
        exchange.Publish(settings,n%4);
    }
    feedback.join();exchange.Apply(settings);done=true;reader.join();
    if(!valid || settings[0].gyroToRightStickPermanent==initialGyro ||
        original->controllers[0].leftStickDeadzone!=0)
        throw std::runtime_error("worker publication consistency / feedback parity / retained snapshot");
    exchange.Light(0,{0,0,0});exchange.Apply(settings);
    if(!settings[0].gameLightbarAvailable)throw std::runtime_error("black game feedback is valid");
    
}
