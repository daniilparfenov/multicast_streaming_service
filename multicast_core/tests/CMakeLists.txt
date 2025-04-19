cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_SOURCE_DIR}/../../ProjectConfig.cmake)

project(multicast_core_lib_test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/../bin)

set(CMAKE_PREFIX_PATH ${CPP_LIB_INSTALL_DIR} ${CMAKE_PREFIX_PATH})
find_package(multicast_core REQUIRED)
find_package(OpenCV REQUIRED)
message(STATUS "Using OpenCV version: ${OpenCV_VERSION}")

add_executable(sender src/sender.cpp)

target_include_directories(sender PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(sender PUBLIC multicast_core::multicast_core ${OpenCV_LIBS})

add_executable(receiver src/receiver.cpp)
target_include_directories(receiver PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(receiver PUBLIC multicast_core::multicast_core ${OpenCV_LIBS})