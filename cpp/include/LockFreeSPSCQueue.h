#ifndef LOCK_FREE_SPSC_QUEUE_H
#define LOCK_FREE_SPSC_QUEUE_H

#include <atomic>
#include <cstddef>

template <typename T, size_t Capacity = 2048>
class LockFreeSPSCQueue {
public:
    LockFreeSPSCQueue() : head(0), tail(0) {};
    
    bool push(const T& item);
    bool pop(T& item);

private:
    T storage[Capacity];

    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
};

#endif