
cmake_minimum_required(VERSION 3.24)

project(testgrpc LANGUAGES CXX)

get_filename_component(test_proto "${CMAKE_CURRENT_LIST_DIR}/test.proto" ABSOLUTE)
get_filename_component(test_proto_path "${test_proto}" PATH)

set(test_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/test.pb.cc")
set(test_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/test.pb.h")
set(test_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/test.grpc.pb.cc")
set(test_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/test.grpc.pb.h")

find_package(protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(absl REQUIRED)

find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_program(_PROTOBUF_PROTOC protoc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

add_custom_command(
      OUTPUT "${test_proto_srcs}" "${test_proto_hdrs}" "${test_grpc_srcs}" "${test_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${test_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${test_proto}"
      DEPENDS "${test_proto}")

add_executable(client client.cpp)
add_executable(server server.cpp)

add_library(test_grpc_proto
  ${test_grpc_srcs}
  ${test_grpc_hdrs}
  ${test_proto_srcs}
  ${test_proto_hdrs})

target_link_libraries(test_grpc_proto
  PUBLIC
  absl::check
  gRPC::grpc++_reflection
  gRPC::grpc++
  protobuf::libprotobuf)

target_include_directories(test_grpc_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(server test_grpc_proto)
target_link_libraries(client test_grpc_proto)

