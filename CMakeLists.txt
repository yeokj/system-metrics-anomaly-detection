cmake_minimum_required(VERSION 3.10)
project(SystemMetricsAnomalyDetection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(cpp/include)
add_executable(AnomalyDetector cpp/src/main.cpp cpp/src/Simulator.cpp cpp/src/DetectionEngine.cpp)

find_package(Threads REQUIRED)
target_link_libraries(AnomalyDetector PRIVATE Threads::Threads)