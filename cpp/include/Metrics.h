#ifndef METRICS_H
#define METRICS_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Enums
enum class FailureMode {
    NONE = 0,
    SPIKE = 1,
    DRIFT = 2,
    CONSTANT_HIGH = 3
};

// Data Carrier
struct TelemetryMetric {
    double timestamp = 0.0;    // High-precision time
    double latency;     // System response time
    double throughput;  // Requests processed
    double error_rate;  // Percentage of failed requests
    int label;          // 0 = Normal, 1 = Anomaly
};

// Logic Provider
class Simulator {
private:
    std::queue<TelemetryMetric> dataQueue;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> isRunning;

    // Internal helper for failure states
    FailureMode currentFailureMode; 

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