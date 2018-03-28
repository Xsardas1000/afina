#include <afina/Executor.h>
#include <cstring>
#include <iostream>


namespace Afina {

    Executor::Executor(std::string _name, size_t _low_watermark, size_t _high_watermark,
                       size_t _max_queue_size, std::chrono::milliseconds _idle_time) {


        name = _name;
        low_watermark = _low_watermark;
        high_watermark = _high_watermark;
        max_queue_size = _max_queue_size;
        idle_time = _idle_time;

        state = State::kRun;

        std::cout << "creating executor\n";
        std::unique_lock<std::mutex> lock(mutex);

        for (int i = 0; i < low_watermark; i++) {
            threads.emplace_back(perform, this);
        }

    }

    void Executor::Stop(bool await) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (state == State::kRun)
            {
                state = State::kStopping;
            }
        }

        std::cout << "stopping\n";

        empty_condition.notify_all();
        if (await) {
            for (size_t i = 0; i < threads.size(); i++) {
                if (threads[i].joinable())
                {
                  // The function returns when the thread execution has completed.
                  //This blocks the execution of the thread that calls this function until the function called on construction returns
                    threads[i].join();
                }
            }
        }

        std::cout << "stop\n";

        state = State::kStopped;
    }

    Executor::~Executor() {
        Stop(true);
    }


    void Executor::perform(Executor *executor) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;


        std::function<void()> task;

        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(executor->mutex);
                executor->num_working--;


                if (executor->empty_condition.wait_for(lock, executor->idle_time, [&executor](void) {
                        return executor->state == Executor::State::kStopping || executor->tasks.empty();
                    })) {

                    if ((executor->threads.size() > executor->low_watermark) &&  executor->tasks.empty() ||
                        executor->state != State::kRun && executor->tasks.empty())
                    {

                          auto curr_thread_id = std::this_thread::get_id();
                          for (size_t i = 0; i < executor->threads.size(); i++)
                          {
                              if (executor->threads[i].get_id() == curr_thread_id)
                              {
                                  executor->threads[i].detach();
                                  auto thread_it = executor->threads.begin() + i;
                                  executor->threads.erase(thread_it);
                                  break;
                              }
                          }
                          return;
                    }
                    continue;
                }


                if (executor->state == Executor::State::kStopped)
                {
                    return;
                }

                executor->num_working++;
                task = executor->tasks.front();
                executor->tasks.pop_front();
            }
            // out of lock
            task();
        }

    }

}
