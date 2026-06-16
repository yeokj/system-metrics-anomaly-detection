#include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "metrics.pb.h"
#include "metrics.grpc.pb.h"
#include "DetectionEngine.h"
#include "LockFreeSPSCQueue.h"

std::atomic<bool> should_shutdown(false);

void SignalHandler(int signum) {
    std::cout << "\n[Server] Shutdown signal (" << signum << ") received. Signaling main thread..." << std::endl;
    should_shutdown.store(true);
}
class AnomalyDetectorServiceImpl final : public telemetry::AnomalyDetectorService::Service {
private:
    DetectionEngine engine;
    LockFreeSPSCQueue<TelemetryMetric, 2048> metricQueue;
    std::thread inferenceWorker;

    void inferenceWorkerLoop() {
        TelemetryMetric native_metric;

        while (!should_shutdown.load()) {
            if (metricQueue.pop(native_metric)) {
                engine.validateMetric(native_metric);
            }
            else {
                std::this_thread::yield();
            }
        }
    }

public:
    AnomalyDetectorServiceImpl() {
        if (!engine.loadConfiguration("python/data/model_params.json")) {
            std::cerr << "[Server Warning] Could not load model_params.json!" << std::endl;
        } 
        else {
            std::cout << "[Server] DetectionEngine successfully loaded model parameters." << std::endl;
            inferenceWorker = std::thread(&AnomalyDetectorServiceImpl::inferenceWorkerLoop, this);
        }
    }

    ~AnomalyDetectorServiceImpl() {
        if (inferenceWorker.joinable()) {
            inferenceWorker.join();
        }
    }

    grpc::Status StreamMetrics(
        grpc::ServerContext* context,
        grpc::ServerReader<telemetry::TelemetryMetric>* reader,
        telemetry::StreamStatus* response
    ) override {
        
        telemetry::TelemetryMetric proto_metric;
        int count = 0;
        int anomalies_detected = 0;

        std::cout << "[Server] Telemetry stream opened by client." << std::endl;

        // Read messages sequentially from the stream until the client calls WritesDone()
        while (reader->Read(&proto_metric)) {
            if (should_shutdown.load()) {
                break;
            }
            ++count;
            
            TelemetryMetric native_metric;
            native_metric.timestamp = proto_metric.timestamp();
            native_metric.latency = proto_metric.latency();
            native_metric.throughput = proto_metric.throughput();
            native_metric.error_rate = proto_metric.error_rate();

            // Execute your ML Inference validation on the live data point
            bool isAnomaly = engine.validateMetric(native_metric);
            
            if (isAnomaly) {
                ++anomalies_detected;
                std::cout << "[ALERT] Anomaly detected! Latency: " << native_metric.latency 
                          << "ms | Error Rate: " << native_metric.error_rate * 100 << "%" << std::endl;
            }
        }

        std::cout << "[Server] Stream ended. Total metrics processed: " << count << std::endl;

        // Populate the final response message back to the client
        response->set_success(true);
        response->set_message("Processed " + std::to_string(count) + " metrics successfully.");

        return grpc::Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    AnomalyDetectorServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    std::cout << "[Server] InferenceServer listening on " << server_address << std::endl;

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    while (!should_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Wait for the server to shut down (blocks the main thread)
    std::cout << "[Server] Shutting down gRPC server safely..." << std::endl;
    server->Shutdown();
    std::cout << "[Server] Main thread exiting gracefully. Goodbye!" << std::endl;
}

int main() {
    RunServer();
    return 0;
}