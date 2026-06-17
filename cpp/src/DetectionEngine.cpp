#include "DetectionEngine.h"
#include <fstream>
#include <cmath>
#include <iostream>

bool DetectionEngine::loadConfiguration(const std::string &configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file at " << configPath << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;

        // Iterate through the keys in your JSON (e.g., "latency", "throughput")
        for (auto& [key, value] : j.items()) {
            ModelParams p;
            p.mean = value.at("mean").get<double>();
            p.std_dev = value.at("std").get<double>(); // Matching Python's 'std' key
            thresholds[key] = p;
        }
    } 
    catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool DetectionEngine::validateMetric(const TelemetryMetric &tm, double zThreshold) {
    bool isAnomalous = false;

    // 1. Check Latency
    if (thresholds.count("latency_roll_mean")) {
        auto& p = thresholds["latency_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.latency - p.mean) / p.std_dev);
            if (z > zThreshold) isAnomalous = true;
        }
    }

    // 2. Check Throughput
    if (!isAnomalous && thresholds.count("throughput_roll_mean")) {
        auto& p = thresholds["throughput_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.throughput - p.mean) / p.std_dev);
            if (z > zThreshold) isAnomalous = true;
        }
    }

    // 3. Check Error Rate
    if (!isAnomalous && thresholds.count("error_rate_roll_mean")) {
        auto& p = thresholds["error_rate_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.error_rate - p.mean) / p.std_dev);
            if (z > zThreshold) isAnomalous = true;
        }
    }

    return isAnomalous;
}

void DetectionEngine::updateWindow(const TelemetryMetric &tm) {
    metricsWindow.push_back(tm);
    int64_t threshold = tm.timestamp - 300;

    while (!metricsWindow.empty() && metricsWindow.front().timestamp < threshold) {
        metricsWindow.pop_front();
    }
}