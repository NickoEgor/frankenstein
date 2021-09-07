set(GRPC_PROTOBUF_PROTOC ${CONAN_BIN_DIRS_PROTOBUF}/protoc)
set(GRPC_CPP_PLUGIN_EXECUTABLE ${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin)

function(add_proto_target TARGET)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "PREFIX" "PROTOS")

  add_library(${TARGET} STATIC EXCLUDE_FROM_ALL)
  target_include_directories(${TARGET} SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CONAN_INCLUDE_DIRS_PROTOBUF} ${CONAN_INCLUDE_DIRS_GRPC})
  target_link_libraries(${TARGET} PRIVATE CONAN_PKG::protobuf CONAN_PKG::grpc)

  set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${ARG_PREFIX})
  file(MAKE_DIRECTORY ${OUTPUT_DIR})

  foreach(PROTO_FILE ${ARG_PROTOS})
    get_filename_component(PROTO_FILE_NAME ${PROTO_FILE} NAME_WE)

    set(PROTO_FILE_OUTPUTS .pb.cc .pb.h .grpc.pb.cc .grpc.pb.h)
    list(TRANSFORM PROTO_FILE_OUTPUTS PREPEND "${OUTPUT_DIR}/${PROTO_FILE_NAME}")

    # --grpc_out "${OUTPUT_DIR}"
    # --cpp_out "${OUTPUT_DIR}"
    add_custom_command(
      OUTPUT ${PROTO_FILE_OUTPUTS}
      COMMAND ${GRPC_PROTOBUF_PROTOC}
              --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
              --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
              -I "${CMAKE_CURRENT_SOURCE_DIR}"
              --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
              ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
      MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
    )

    target_sources(${TARGET} PRIVATE ${PROTO_FILE_OUTPUTS})
  endforeach()
endfunction()
