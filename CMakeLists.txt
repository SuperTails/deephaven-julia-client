project(JuliaWrap)

cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(JlCxx)
get_target_property(JlCxx_location JlCxx::cxxwrap_julia LOCATION)
get_filename_component(JlCxx_location ${JlCxx_location} DIRECTORY)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${JlCxx_location}")

#add_subdirectory(/deephaven/deephaven-core/cpp-client/deephaven deephaven_dir)

message(STATUS "Found JlCxx at ${JlCxx_location}")

find_package(Arrow REQUIRED)
find_package(ArrowFlight REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(Immer REQUIRED)

# TODO: This is a workaround because currently the C++ client doesn't link properly when used as a package.
add_subdirectory(${DHCLIENT_DIR} deephaven_dir)

# TODO: When the deephaven package can be built separately, this should be the method used.
#find_package(deephaven REQUIRED)

add_library(dhwrapper SHARED wrap.cpp)

target_link_libraries(dhwrapper JlCxx::cxxwrap_julia JlCxx::cxxwrap_julia_stl deephaven::client)

install(TARGETS
  dhwrapper
LIBRARY DESTINATION lib
ARCHIVE DESTINATION lib
RUNTIME DESTINATION lib)