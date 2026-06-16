#ifndef LOCK_FREE_SPSC_QUEUE_H
#define LOCK_FREE_SPSC_QUEUE_H

#include <atomic>
#include <cstddef>

template <typename T, size_t Capacity = 2048>
class LockFreeSPSCQueue {
public:
    LockFreeSPSCQueue() : head(0), tail(0) {};
    
    bool push(const T &item);
    bool pop(T &item);

private:
    T storage[Capacity];

    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
};

template <typename T, size_t Capacity>
bool LockFreeSPSCQueue<T, Capacity>::push(const T &item) {
    size_t currTail = tail.load(std::memory_order_relaxed);
    size_t nextTail = (currTail + 1) & (Capacity - 1);

    size_t currHead = head.load(std::memory_order_acquire);

    if (nextTail == currHead) return false;

    storage[currTail] = item;
    tail.store(nextTail, std::memory_order_release);

    return true;
}

template <typename T, size_t Capacity>
bool LockFreeSPSCQueue<T, Capacity>::pop(T &item) {
    // Your pop logic goes here
    size_t currHead = head.load(std::memory_order_relaxed);
    size_t currTail = tail.load(std::memory_order_acquire);

    if (currHead == currTail) return false;

    item = storage[currHead];

    size_t nextHead = (currHead + 1) & (Capacity - 1);
    head.store(nextHead, std::memory_order_release);

    return true;
}

#endif