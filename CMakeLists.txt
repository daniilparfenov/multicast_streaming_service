cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_SOURCE_DIR}/ProjectConfig.cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

project(multicast_streaming_service)

find_package(OpenCV REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/multicast_core/include/multicast_core_bits")
include_directories("${CMAKE_SOURCE_DIR}/pybindings")

file (GLOB SRC_FILES "multicast_core/src/*.cpp")
file (GLOB HEADER_FILES "multicast_core/include/multicast_core_bits/*.h")
file (GLOB PYBINDINGS_FILES "pybindings/*.cpp" "pybindings/*.h")

include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(multicast_core 
	${SRC_FILES}
	${HEADER_FILES}
	${PYBINDINGS_FILES}
)

target_link_libraries(multicast_core PUBLIC ${OpenCV_LIBS})

install(TARGETS multicast_core
  COMPONENT python
  LIBRARY DESTINATION "${PYTHON_LIB_INSTALL_DIR}"
)
