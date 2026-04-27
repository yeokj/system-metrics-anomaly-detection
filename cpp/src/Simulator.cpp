#include "Metrics.h"
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>

Simulator::Simulator() : isRunning(false), currentFailureMode(0) {}

Simulator::~Simulator() {
    stop();
}

void Simulator::trafficGenerator() {
    // 1. Setup Random Engine
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 2. Define Normal Distribution (Mean = 15ms, StdDev = 2.0)
    std::normal_distribution<double> latencyDist(15.0, 2.0);
    std::normal_distribution<double> throughputDist(100.0, 10.0);

    while (isRunning) {
        Metric m;
        // Use high_resolution_clock for the research-grade precision
        auto now = std::chrono::steady_clock::now();
        m.timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        m.latency = latencyDist(gen);
        m.throughput = throughputDist(gen);
        m.error_rate = 0.01; // Baseline 1% error
        m.label = 0;         // Normal data

        // 3. Thread-Safe Push
        {
            std::lock_guard<std::mutex> lock(mtx);
            dataQueue.push(m);
        }
        cv.notify_one(); // Wake up the logger

        // Sleep to simulate a 10Hz sampling rate (100ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Simulator::failureInjector() {

}

void Simulator::metricsLogger() {
    std::ofstream file("../python/data/raw_metrics.csv");
    file << "timestamp,latency,throughput,error_rate,label\n";

    while (isRunning || !dataQueue.empty()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !dataQueue.empty() || !isRunning; });

        if (!dataQueue.empty()) {
            Metric m;
            m = dataQueue.front();
            dataQueue.pop();

            lock.unlock();

            file << std::fixed << std::setprecision(6);
            file << m.timestamp << "," << m.latency << "," << m.throughput << "," << m.error_rate << "," << m.label << "\n";
        }
    }
    file.close();
}