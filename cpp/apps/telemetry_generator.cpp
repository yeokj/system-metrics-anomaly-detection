#include "Metrics.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include "metrics.pb.h"
#include "metrics.grpc.pb.h"

int main() {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    std::unique_ptr<telemetry::AnomalyDetectorService::Stub> stub = telemetry::AnomalyDetectorService::NewStub(channel);

    grpc::ClientContext context;
    telemetry::StreamStatus response;

    auto writer = stub->StreamMetrics(&context, &response);

    Simulator sim;
    sim.run(writer.get());
}