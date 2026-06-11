#include "Metrics.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <iostream>
#include "metrics.pb.h"
#include "metrics.grpc.pb.h"

int main(int argc, char* argv[]) {
    std::string target_address = "localhost:50051";

    if (argc > 1) {
        target_address = std::string(argv[1]) + ":50051";
    }

    std::cout << "[Client] Connecting to gRPC server at: " << target_address << std::endl;

    auto channel = grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials());
    auto stub = telemetry::AnomalyDetectorService::NewStub(channel);

    Simulator sim;

    grpc::ClientContext context;
    telemetry::StreamStatus response;
    std::unique_ptr<grpc::ClientWriter<telemetry::TelemetryMetric>> writer(
        stub->StreamMetrics(&context, &response)
    );

    sim.run(writer.get());

    return 0;
}