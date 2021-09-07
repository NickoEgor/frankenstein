cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

project(frankenstein CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
  add_compile_options(-g)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
    "${CMAKE_BINARY_DIR}/conan.cmake"
    TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_run(
  CONANFILE conanfile.py
  BUILD_TYPE ${CMAKE_BUILD_TYPE}
  BUILD missing
  BASIC_SETUP CMAKE_TARGETS KEEP_RPATHS)

include(cmake/proto_utils.cmake)
add_proto_target(exchange_service_proto PREFIX proto PROTOS proto/exchange_service.proto)

add_subdirectory(frankenstein)
