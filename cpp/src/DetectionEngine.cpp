#include "DetectionEngine.h"
#include <fstream>
#include <cmath>
#include <pqxx/pqxx>
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
    AnomalyAlert alert;
    alert.timestamp = static_cast<int64_t>(tm.timestamp);
    bool isAnomalous = false;

    // 1. Check Latency
    if (thresholds.count("latency_roll_mean")) {
        auto& p = thresholds["latency_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.latency - p.mean) / p.std_dev);
            if (z > zThreshold) { 
                isAnomalous = true;
                alert.metric_type = "latency";
                alert.violated_value = tm.latency;
                alert.threshold_boundary = p.mean;
            }
        }
    }

    // 2. Check Throughput
    if (!isAnomalous && thresholds.count("throughput_roll_mean")) {
        auto& p = thresholds["throughput_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.throughput - p.mean) / p.std_dev);
            if (z > zThreshold) { 
                isAnomalous = true;
                alert.metric_type = "throughput";
                alert.violated_value = tm.throughput;
                alert.threshold_boundary = p.mean;
            }
        }
    }

    // 3. Check Error Rate
    if (!isAnomalous && thresholds.count("error_rate_roll_mean")) {
        auto& p = thresholds["error_rate_roll_mean"];
        if (p.std_dev > 0) {
            double z = std::abs((tm.error_rate - p.mean) / p.std_dev);
            if (z > zThreshold) { 
                isAnomalous = true;
                alert.metric_type = "error_rate";
                alert.violated_value = tm.error_rate;
                alert.threshold_boundary = p.mean;
            }
        }
    }

    if (isAnomalous) {
        std::lock_guard<std::mutex> lock(alertMutex_);
        alertQueue_.push(alert);
        alertCv_.notify_one();
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

void DetectionEngine::dbWorkerLoop(const std::atomic<bool> &should_shutdown) {
    std::cout << "[DB CLOUD WORKER] Launching asynchronous database consumer thread..." << std::endl;

    try {
        // Establish Secure Remote Connection to Supabase Connection Pooler
        const char* env_conn = std::getenv("SUPABASE_CONN_STR");
        std::string conn_str;

        if (env_conn != nullptr) {
            conn_str = std::string(env_conn);
        } 
        else {
            std::cerr << "[DB CLOUD WORKER ERROR] SUPABASE_CONN_STR environment variable is NOT set!" << std::endl;
            return;
        }

        // Append the security mode if it's not already in the string
        if (conn_str.find("sslmode=") == std::string::npos) {
            if (conn_str.find("?") == std::string::npos) {
                conn_str += "?sslmode=require";
            } else {
                conn_str += "&sslmode=require";
            }
        }
        
        pqxx::connection c(conn_str);
        if (c.is_open()) {
            std::cout << "[DB CLOUD WORKER] Successfully established persistent WAN tunnel to Supabase!" << std::endl;
        }

        // Provision Storage Schema Remotely (Idempotent Execution)
        pqxx::work schema_tx(c);
        schema_tx.exec(
            "CREATE TABLE IF NOT EXISTS anomaly_alerts ("
            "id SERIAL PRIMARY KEY,"
            "timestamp BIGINT NOT NULL,"
            "metric_type TEXT NOT NULL,"
            "violated_value DOUBLE PRECISION NOT NULL,"
            "threshold_boundary DOUBLE PRECISION NOT NULL,"
            "created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP"
            ");"
        );
        schema_tx.commit();

        while (!should_shutdown.load()) {
            std::unique_lock<std::mutex> lock(alertMutex_);
            alertCv_.wait(lock, [this, &should_shutdown]() { 
                return !alertQueue_.empty() || should_shutdown.load(); 
            });
            // Break out if shutdown is requested and no more alerts are left to write
            if (alertQueue_.empty() && should_shutdown.load()) {
                break;
            }
            AnomalyAlert alert = alertQueue_.front();
            alertQueue_.pop();
            lock.unlock();
            
            try {
                // Stream the record straight across the internet via transaction contexts
                pqxx::work write_tx(c);
                write_tx.exec(
                    "INSERT INTO anomaly_alerts (timestamp, metric_type, violated_value, threshold_boundary) VALUES ($1, $2, $3, $4);", 
                    pqxx::params{
                        alert.timestamp, 
                        alert.metric_type, 
                        alert.violated_value, 
                        alert.threshold_boundary
                    }
                );
                write_tx.commit(); // Finishes cloud synchronization transaction cleanly
                
            } 
            catch (const std::exception &write_err) {
                std::cerr << "[DB CLOUD WORKER WRITE FAILURE] Transaction aborted: " << write_err.what() << std::endl;
                // In enterprise systems, you would route failed metrics to a local dead-letter disk cache here
            }
        }
    } 
    catch (const std::exception &e) {
        std::cerr << "[DB CLOUD WORKER FATAL ERROR] Connection path dropped: " << e.what() << std::endl;
    }
        
   std::cout << "[DB CLOUD WORKER] Asynchronous Cloud Postgres consumer thread exited cleanly." << std::endl;
}

void DetectionEngine::stopAlertWorker() {
    alertCv_.notify_all();
}