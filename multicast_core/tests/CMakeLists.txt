cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_SOURCE_DIR}/../../ProjectConfig.cmake)

project(multicast_core_lib_test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/../bin)

find_library(MULTICAST_CORE_LIB multicast_core HINTS ${CPP_LIB_INSTALL_DIR}/lib)
include_directories(${CPP_LIB_INSTALL_DIR}/include)

add_executable(test src/test.cpp)

target_link_libraries(test PUBLIC ${MULTICAST_CORE_LIB})
