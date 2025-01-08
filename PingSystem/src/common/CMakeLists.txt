# Proto file
get_filename_component(PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/notify.proto" ABSOLUTE)
get_filename_component(PROTOS_PATH "${PROTOS}" PATH)

# Generated sources
set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/notify.pb.h")
set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/notify.pb.cc")
set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/notify.grpc.pb.cc")
set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/notify.grpc.pb.h")
add_custom_command(
      OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${PROTOS_PATH}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${PROTOS}"
      DEPENDS "${PROTOS}")

set(PROTO_GRPC_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)  # PARENT_SCOPE for delivering the variable to the parent layer
add_custom_target(protos DEPENDS ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_HDRS} ${GRPC_SRCS})

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(notify_grpc_proto
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_HDRS}
    ${GRPC_SRCS})

target_link_libraries(notify_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
