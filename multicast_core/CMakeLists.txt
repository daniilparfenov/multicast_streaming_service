cmake_minimum_required(VERSION 3.16)
project(multicast_core VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_SOURCE_DIR}/../ProjectConfig.cmake)

set(CMAKE_INSTALL_PREFIX ${CPP_LIB_INSTALL_DIR})

set(PROJECT_SRC_DIR "src")
set(PROJECT_INCLUDE_DIR "include/multicast_core_bits")

# Source files
set(SRC_FILES
    ${PROJECT_INCLUDE_DIR}/receiver.h
    ${PROJECT_INCLUDE_DIR}/sender.h
    ${PROJECT_SRC_DIR}/receiver.cpp
    ${PROJECT_SRC_DIR}/sender.cpp
)

# Add library
add_library(multicast_core SHARED ${SRC_FILES})

# Include directories
target_include_directories(multicast_core PRIVATE include/multicast_core_bits)

# Install
install(TARGETS multicast_core DESTINATION lib)

# Install the headers
install(FILES include/multicast_core.h DESTINATION include)

# Create base directory
install(DIRECTORY include/multicast_core_bits DESTINATION include)
