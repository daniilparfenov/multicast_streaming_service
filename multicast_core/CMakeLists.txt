cmake_minimum_required(VERSION 3.16)
project(multicast_core VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_SOURCE_DIR}/../ProjectConfig.cmake)

set(CMAKE_INSTALL_PREFIX ${CPP_LIB_INSTALL_DIR})

set(PROJECT_SRC_DIR "src")
set(PROJECT_INCLUDE_DIR "include/multicast_core_bits")

# OpenCV include
find_package(OpenCV REQUIRED)

# Source files
set(SRC_FILES
    ${PROJECT_INCLUDE_DIR}/receiver.h
    ${PROJECT_INCLUDE_DIR}/sender.h
    ${PROJECT_SRC_DIR}/receiver.cpp
    ${PROJECT_SRC_DIR}/sender.cpp
)

# Add library
add_library(multicast_core SHARED ${SRC_FILES})
target_link_libraries(multicast_core PUBLIC ${OpenCV_LIBS})

# Include directories
target_include_directories(multicast_core
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/multicast_core_bits>
        $<INSTALL_INTERFACE:include>
        ${OpenCV_INCLUDE_DIRS}
)

# Install
install(TARGETS multicast_core
    EXPORT multicast_coreTargets
    DESTINATION lib
)

install(EXPORT multicast_coreTargets
    FILE multicast_coreTargets.cmake
    NAMESPACE multicast_core::
    DESTINATION lib/cmake/multicast_core
)

# Install the headers
install(FILES include/multicast_core.h DESTINATION include)
install(DIRECTORY include/multicast_core_bits DESTINATION include)

# === Добавляем пакетный конфиг ===

include(CMakePackageConfigHelpers)

set(CONFIG_INSTALL_DIR lib/cmake/multicast_core)

# пути для подстановки в config
set(PACKAGE_INCLUDE_INSTALL_DIR "include")
set(PACKAGE_LIB_INSTALL_DIR "lib")

# Генерация multicast_coreConfig.cmake из шаблона
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/multicast_coreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/multicast_coreConfig.cmake"
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

# Установка финального config файла
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/multicast_coreConfig.cmake"
    DESTINATION ${CONFIG_INSTALL_DIR}
)