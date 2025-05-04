cmake_minimum_required(VERSION 3.16)

project(multicast_streaming_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

include_directories(
    "${CMAKE_SOURCE_DIR}/multicast_core/include/multicast_core_bits"
    "${CMAKE_SOURCE_DIR}/pybindings"
)

file(GLOB SRC_FILES "multicast_core/src/*.cpp")
file(GLOB HEADER_FILES "multicast_core/include/multicast_core_bits/*.h")
file(GLOB PYBINDINGS_FILES "pybindings/*.cpp")

include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)

add_library(multicast_core SHARED ${SRC_FILES} ${PYBINDINGS_FILES})
target_link_libraries(multicast_core PRIVATE ${OpenCV_LIBS} Threads::Threads)

if(WIN32)
    target_link_libraries(multicast_core PRIVATE ws2_32)
endif()

pybind11_add_module(multicast_core_pybind ${PYBINDINGS_FILES})
target_link_libraries(multicast_core_pybind PRIVATE multicast_core)

install(TARGETS multicast_core_pybind
    DESTINATION "${CMAKE_INSTALL_PREFIX}/python"
)
