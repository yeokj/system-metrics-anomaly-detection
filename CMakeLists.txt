cmake_minimum_required(VERSION 3.10)
project(SystemMetricsAnomalyDetection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Protobuf compiler and libraries, and gRPC package and its internal C++ plugin target
find_package(gRPC CONFIG QUIET)
if(gRPC_FOUND)
    message(STATUS "Found gRPC via CMake Config")
    set(GRPC_LIBRARIES gRPC::grpc++)
    set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
    # Fallback to pkg-config if CMake Config files are missing (works on Ubuntu Apt)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GRPC REQUIRED grpc++)
    message(STATUS "Found gRPC via PkgConfig")
    set(GRPC_LIBRARIES ${GRPC_LIBRARIES})

    find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

find_package(Protobuf REQUIRED)

# Paths to store the generated source files
set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/metrics.pb.cc")
set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/metrics.pb.h")
set(GRPC_SRCS  "${CMAKE_CURRENT_BINARY_DIR}/metrics.grpc.pb.cc")
set(GRPC_HDRS  "${CMAKE_CURRENT_BINARY_DIR}/metrics.grpc.pb.h")

# Custom build command that triggers protoc during compilation
add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
         --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
         -I "${CMAKE_CURRENT_SOURCE_DIR}/cpp/proto"
         "${CMAKE_CURRENT_SOURCE_DIR}/cpp/proto/metrics.proto"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpp/proto/metrics.proto"
)

# Target Definitions
add_executable(TelemetryGenerator 
    cpp/apps/telemetry_generator.cpp 
    cpp/src/Simulator.cpp
    cpp/src/DetectionEngine.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
add_executable(InferenceServer 
    cpp/apps/inference_server.cpp 
    cpp/src/DetectionEngine.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Target Include Paths (Configured after targets are created)
target_include_directories(TelemetryGenerator PRIVATE cpp/include ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(InferenceServer PRIVATE cpp/include ${CMAKE_CURRENT_BINARY_DIR})

# System Library Linkage
find_package(Threads REQUIRED)
target_link_libraries(TelemetryGenerator 
    PRIVATE 
        Threads::Threads
        ${GRPC_LIBRARIES}
        protobuf::libprotobuf
)
target_link_libraries(InferenceServer 
    PRIVATE 
    Threads::Threads
    ${GRPC_LIBRARIES}
    protobuf::libprotobuf
)