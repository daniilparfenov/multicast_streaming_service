cmake_minimum_required(VERSION 3.16)
project(multicast_streaming_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Указываем явные пути к исходным файлам
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/multicast_core/src")
set(PYBINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pybindings")

# Собираем исходники
file(GLOB SRC_FILES "${SRC_DIR}/*.cpp")
file(GLOB PYBINDINGS_FILES "${PYBINDINGS_DIR}/*.cpp")

include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)

# Основная библиотека
add_library(multicast_core SHARED 
    ${SRC_FILES}
    ${PYBINDINGS_FILES}
)

target_include_directories(multicast_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/multicast_core/include/multicast_core_bits"
)

target_link_libraries(multicast_core 
    PRIVATE 
    ${OpenCV_LIBS} 
    pthread
)

# Python-модуль
pybind11_add_module(multicast_core_pybind 
    ${PYBINDINGS_FILES}
    ${SRC_FILES}
)

target_link_libraries(multicast_core_pybind 
    PRIVATE 
    multicast_core
)
