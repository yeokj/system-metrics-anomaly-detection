#ifndef METRICS_H
#define METRICS_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// 1. Data Carrier
struct Metric {
    double timestamp = 0.0;    // High-precision time
    double latency;     // System response time
    double throughput;  // Requests processed
    double error_rate;  // Percentage of failed requests
    int label;          // 0 = Normal, 1 = Anomaly
};

// 2. Logic Provider
class Simulator {
private:
    std::queue<Metric> dataQueue;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> isRunning;

    // Internal helper for failure states
    int currentFailureMode; 

public:
    Simulator(); // Constructor
    virtual ~Simulator(); // Deconstructor
    
    // Core Threads
    void trafficGenerator();
    void failureInjector();
    void metricsLogger();

    // Controller
    void run();
    void stop();
};

#endif