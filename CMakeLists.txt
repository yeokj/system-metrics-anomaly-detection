cmake_minimum_required(VERSION 3.10)
project(SystemMetricsAnomalyDetection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target Definitions
add_executable(TelemetryGenerator cpp/apps/telemetry_generator.cpp cpp/src/Simulator.cpp)
add_executable(InferenceServer cpp/apps/inference_server.cpp cpp/src/DetectionEngine.cpp)

# Target Include Paths (Configured after targets are created)
target_include_directories(TelemetryGenerator PRIVATE cpp/include)
target_include_directories(InferenceServer PRIVATE cpp/include)

# System Library Linkage
find_package(Threads REQUIRED)
target_link_libraries(TelemetryGenerator PRIVATE Threads::Threads)
target_link_libraries(InferenceServer PRIVATE Threads::Threads)