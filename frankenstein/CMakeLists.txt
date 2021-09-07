find_package(Qt5 REQUIRED Core)

file(GLOB sources "src/*.cpp")
list(REMOVE_ITEM sources src/main_client.cpp)
list(REMOVE_ITEM sources src/main_server.cpp)

# library
add_library(frankenstein_lib STATIC)
target_include_directories(frankenstein_lib PUBLIC include)
# target_link_libraries(frankenstein_lib PUBLIC exchange_service_proto CONAN_PKG::grpc CONAN_PKG::qt)
target_link_libraries(frankenstein_lib PUBLIC exchange_service_proto CONAN_PKG::grpc Qt5::Core)
set_target_properties(frankenstein_lib PROPERTIES AUTOMOC ON)
target_sources(frankenstein_lib PRIVATE ${sources}
  include/frankenstein/server.hpp
  include/frankenstein/client.hpp)

# executable server
add_executable(server "src/main_server.cpp")
# target_compile_options(server PUBLIC -fPIC)
target_link_libraries(server PRIVATE frankenstein_lib)
install(TARGETS server RUNTIME DESTINATION bin)

# executable client
add_executable(client "src/main_client.cpp")
# target_compile_options(client PUBLIC -fPIC)
target_link_libraries(client PRIVATE frankenstein_lib)
install(TARGETS client RUNTIME DESTINATION bin)
