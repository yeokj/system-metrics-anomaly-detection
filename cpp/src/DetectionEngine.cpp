#include "DetectionEngine.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <sqlite3.h>

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
    sqlite3 *db = nullptr;
    
    // Open (or create) the anomalies.db local relational file
    if (sqlite3_open("anomalies.db", &db) != SQLITE_OK) {
        std::cerr << "[DB WORKER ERROR] Failed to open local database: " << sqlite3_errmsg(db) << std::endl;
        if (db) sqlite3_close(db);
        return;
    }
    
    // Auto-initialize the relational table schema on startup
    const char *createTableSQL = 
        "CREATE TABLE IF NOT EXISTS anomaly_alerts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp BIGINT NOT NULL,"
        "metric_type TEXT NOT NULL,"
        "violated_value REAL NOT NULL,"
        "threshold_boundary REAL NOT NULL"
        ");";
        
    char *errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "[DB WORKER ERROR] Failed to initialize table schema: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    } 
    else {
        std::cout << "[DB WORKER] SQLite database and schema verified successfully." << std::endl;
    }

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

        sqlite3_stmt *stmt = nullptr;
        const char *insertSQL = "INSERT INTO anomaly_alerts (timestamp, metric_type, violated_value, threshold_boundary) VALUES (?, ?, ?, ?);";
        
        if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr) == SQLITE_OK) {
            // Bind fields (1-indexed parameters matching the '?' placeholders)
            sqlite3_bind_int64(stmt, 1, alert.timestamp);
            sqlite3_bind_text(stmt, 2, alert.metric_type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 3, alert.violated_value);
            sqlite3_bind_double(stmt, 4, alert.threshold_boundary);
            
            // Execute statement execution step
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "[DB WORKER ERROR] Failed to execute anomaly alert insert: " 
                          << sqlite3_errmsg(db) << std::endl;
            }
            
            // Destroy statement handle to clear temporary memory
            sqlite3_finalize(stmt);
        } 
        else {
            std::cerr << "[DB WORKER ERROR] Failed to prepare insert statement: " 
                      << sqlite3_errmsg(db) << std::endl;
        }
    }

    // Safely release file-descriptor connection locks before exiting thread execution context
    if (db) {
        sqlite3_close(db);
        std::cout << "[DB WORKER] SQLite database connection closed cleanly." << std::endl;
    }
}

void DetectionEngine::stopAlertWorker() {
    alertCv_.notify_all();
}