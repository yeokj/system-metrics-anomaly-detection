#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "metrics.pb.h"
#include "metrics.grpc.pb.h"
#include "DetectionEngine.h"

// We will bring in your DetectionEngine shortly!

class AnomalyDetectorServiceImpl final : public telemetry::AnomalyDetectorService::Service {
private:
    DetectionEngine engine;
    
public:
    AnomalyDetectorServiceImpl() {
        if (!engine.loadConfiguration("python/data/model_params.json")) {
            std::cerr << "[Server Warning] Could not load model_params.json!" << std::endl;
        } else {
            std::cout << "[Server] DetectionEngine successfully loaded model parameters." << std::endl;
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
    // Listen on the given address without authentication mechanisms for now
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register our service implementation
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "[Server] InferenceServer listening on " << server_address << std::endl;

    // Wait for the server to shut down (blocks the main thread)
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}