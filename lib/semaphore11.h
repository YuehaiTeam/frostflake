#ifndef SEMAPHORE11_H
#define SEMAPHORE11_H
#include <condition_variable>
#include <mutex>

class Semaphore {
public:
    Semaphore(int count = 0) : count_(count) {}
    void notify() {
        std::unique_lock<std::mutex> lk(mutex_);
        count_++;
        cv_.notify_one();
    }
    void wait() {
        std::unique_lock<std::mutex> lk(mutex_);
        while (count_ == 0) {
            cv_.wait(lk);
        }
        count_--;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

#endif