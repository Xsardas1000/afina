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

//        for (int i = 0; i < low_watermark; i++)
//        {
//            pthread_t thread_id;
//            if (pthread_create(&thread_id, nullptr, perform, this) < 0) {
//                throw std::runtime_error("Could not create thread");
//            }
//            threads.emplace_back(thread_id); //push_back doesn't work
//        }
    }

    void Executor::Stop(bool await) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

        {
            std::unique_lock<std::mutex> lock(mutex);
            if (state == State::kRun) {
                state = State::kStopping;
            }
        }

        //send to all threads that we are going to stop
        empty_condition.notify_all();
        if (await)
        {
            std::unique_lock<std::mutex> lock(mutex);

            //wait all threads to finish
            std::cout << threads.size() << std::endl;
            while (!threads.empty()) {
                empty_condition.wait(lock);
            }

            //all threads completed
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

                while (executor->state == Executor::State::kRun && executor->tasks.empty())
                {
                    executor->empty_condition.wait(lock);
                }

                auto res = executor->empty_condition.wait_for(lock,
                                                                executor->idle_time,
                                                            [&executor]() {return (!executor->tasks.empty() && executor->state != Executor::State::kStopping);});
                if ((!res && (executor->threads.size() > executor->low_watermark)) ||
                        (executor->state == Executor::State::kStopped))
                {
                    if (executor->threads.size() > executor->low_watermark)
                    // kill current thread

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
                    }
                    return;
                }


                executor->num_working++;
                task = executor->tasks.front();
                executor->tasks.pop_front();
            }
            // out of lock
            std::cout << "going to start task\n";

            task();
            std::cout << "completed task\n";
        }

    }

}