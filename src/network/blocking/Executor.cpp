#include <afina/Executor.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <cstring>
#include <iostream>


namespace Afina {

Executor::Executor(std::string _name, size_t _low_watermark, size_t _high_watermark,
                   size_t _max_queue_size, std::chrono::milliseconds _idle_time)
{

    name = _name;
    low_watermark = _low_watermark;
    hight_watermark = _high_watermark;
    max_queue_size = _max_queue_size;
    idle_time = _idle_time;
    state = State::kRun;


    std::cout << "pool: " << name << " " << __PRETTY_FUNCTION__ << std::endl;
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = 0; i < low_watermark; i++)
    {
        threads.emplace_back(perform, this);
    }
}

void Executor::Stop(bool await) {
    std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (state == State::kRun)
        {
            state = State::kStopping;
        }
    }

}

Executor::~Executor() {
    std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
    Stop(true);
}

void perform(Executor *executor) {
    std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
    std::function<void()> task;
    
}


