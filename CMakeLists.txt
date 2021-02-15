cmake_minimum_required(VERSION 3.7)
project(dcm2itk VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(minizip-ng)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})
include_directories(tclap/include)

configure_file(config.h.in config.h @ONLY)
include_directories("${PROJECT_BINARY_DIR}") # for config.h
add_executable(dcm2itk main.cpp)
target_link_libraries(dcm2itk ${ITK_LIBRARIES} minizip)
