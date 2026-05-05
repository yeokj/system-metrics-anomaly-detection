#include "Metrics.h"
#include "DetectionEngine.h"
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>
#include <iostream>

Simulator::Simulator() : isRunning(false), currentFailureMode(FailureMode::NONE) {}

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

    double driftAccumulator = 0.0;

    while (isRunning) {
        Metric m;
        // Use high_resolution_clock for the research-grade precision
        auto now = std::chrono::steady_clock::now();
        m.timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        m.latency = latencyDist(gen);
        m.throughput = throughputDist(gen);
        m.error_rate = 0.01; // Baseline 1% error
        m.label = 0;         // Normal data

        switch (currentFailureMode) {
            case FailureMode::NONE:
                driftAccumulator = 0.0;
                break;
            case FailureMode::SPIKE:
                m.latency += 100.0;
                m.label = 1;
                break;
            case FailureMode::DRIFT:
                driftAccumulator += 1.5;
                m.latency += driftAccumulator;
                m.label = 1;
                break;
            case FailureMode::CONSTANT_HIGH:
                m.latency += 40.0;
                m.label = 1;
                break;
        }

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
    // Baseline
    currentFailureMode = FailureMode::NONE;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Sudden Spike
    currentFailureMode = FailureMode::SPIKE;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Recovery
    currentFailureMode = FailureMode::NONE;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // The Growing Leak
    currentFailureMode = FailureMode::DRIFT;
}

void Simulator::metricsLogger() {
    DetectionEngine engine;
    if (!engine.loadConfiguration("python/data/model_params.json")) {
        std::cerr << "Phase 3 Error: Could not load model_params.json" << std::endl;
        return;
    }

    std::ofstream file("python/data/raw_metrics.csv");
    file << "timestamp,latency,throughput,error_rate,label,detected_anomaly,overhead_us\n";

    double totalOverhead = 0.0;
    int checkCount = 0;

    while (isRunning || !dataQueue.empty()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !dataQueue.empty() || !isRunning; });

        if (!dataQueue.empty()) {
            Metric m;
            m = dataQueue.front();
            dataQueue.pop();
            lock.unlock();

            // Execute & Time the Detection Engine (Research Variable)
            auto start = std::chrono::high_resolution_clock::now();
            
            bool isDetected = engine.validateMetric(m);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            totalOverhead += duration;
            checkCount++;

            file << std::fixed << std::setprecision(6);
            file << m.timestamp << "," << m.latency << "," << m.throughput << "," << m.error_rate << "," << m.label << "," << (isDetected ? 1 : 0) << "," << duration << "\n";
        }
    }

    // Output Research Summary
    if (checkCount > 0) {
        std::cout << "\n--- Phase 3 Research Results ---" << std::endl;
        std::cout << "Total Checks Performed: " << checkCount << std::endl;
        std::cout << "Avg CPU Overhead: " << (totalOverhead / checkCount) << " us" << std::endl;
        std::cout << "--------------------------------\n" << std::endl;
    }

    file.close();
}

void Simulator::run() {
    isRunning = true;

    std::thread worker1(&Simulator::trafficGenerator, this);
    std::thread worker2(&Simulator::metricsLogger, this);
    std::thread worker3(&Simulator::failureInjector, this);

    // Let the simulation collect data for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Signal the threads to finish
    stop(); 

    // Now it's safe to join, because stop() broke the while loops
    worker1.join();
    worker2.join();
    worker3.join();
}

void Simulator::stop() {
    isRunning = false;

    cv.notify_all();
}