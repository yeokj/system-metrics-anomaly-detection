#include "Metrics.h"
#include "DetectionEngine.h"
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include "metrics.pb.h"
#include "metrics.grpc.pb.h"

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
        TelemetryMetric tm;
        // Use high_resolution_clock for the research-grade precision
        auto now = std::chrono::steady_clock::now();
        tm.timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        tm.latency = latencyDist(gen);
        tm.throughput = throughputDist(gen);
        tm.error_rate = 0.01; // Baseline 1% error
        tm.label = 0;         // Normal data

        switch (currentFailureMode) {
            case FailureMode::NONE:
                driftAccumulator = 0.0;
                break;
            case FailureMode::SPIKE:
                tm.latency += 100.0;
                tm.label = 1;
                break;
            case FailureMode::DRIFT:
                driftAccumulator += 1.5;
                tm.latency += driftAccumulator;
                tm.label = 1;
                break;
            case FailureMode::CONSTANT_HIGH:
                tm.latency += 40.0;
                tm.label = 1;
                break;
        }

        // 3. Thread-Safe Push
        {
            std::lock_guard<std::mutex> lock(mtx);
            dataQueue.push(tm);
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

void Simulator::metricsLogger(grpc::ClientWriter<telemetry::TelemetryMetric>* writer) {
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
        if (!dataQueue.empty()) {
            TelemetryMetric tm;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]{ return !dataQueue.empty() || !isRunning; });
                tm = dataQueue.front();
                dataQueue.pop();
            }

            if (writer != nullptr) {
                telemetry::TelemetryMetric proto_packet;
                
                // Map your local queue values (tm) directly to the protobuf setters
                proto_packet.set_timestamp(tm.timestamp);
                proto_packet.set_latency(tm.latency);
                proto_packet.set_throughput(tm.throughput);
                proto_packet.set_error_rate(tm.error_rate);

                // Fire the message over the network stream
                writer->Write(proto_packet);
            }

            // Execute & Time the Detection Engine (Research Variable)
            auto start = std::chrono::high_resolution_clock::now();
            
            bool isDetected = engine.validateMetric(tm);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            totalOverhead += duration;
            checkCount++;

            file << std::fixed << std::setprecision(6);
            file << tm.timestamp << "," 
            << tm.latency << "," 
            << tm.throughput << "," 
            << tm.error_rate << "," 
            << tm.label << "," 
            << (isDetected ? 1 : 0) << "," 
            << duration << "\n";
        }
    }

    // Notify the server we are closing the stream:
    if (writer != nullptr) {
        writer->WritesDone();
    }

    // Output Summary
    if (checkCount > 0) {
        std::cout << "\n--- Telemetry Inference Summary ---" << std::endl;
        std::cout << "Total Checks Performed: " << checkCount << std::endl;
        std::cout << "Avg CPU Overhead: " << (totalOverhead / checkCount) << " us" << std::endl;
        std::cout << "--------------------------------\n" << std::endl;
    }

    file.close();
}

void Simulator::run(grpc::ClientWriter<telemetry::TelemetryMetric>* writer) {
    isRunning = true;

    std::thread worker1(&Simulator::trafficGenerator, this);
    std::thread worker2(&Simulator::metricsLogger, this, writer);
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