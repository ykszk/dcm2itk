cmake_minimum_required(VERSION 2.8)
project(dcm2itk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(minizip-ng)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

add_executable(dcm2itk main.cpp)
target_link_libraries(dcm2itk ${ITK_LIBRARIES} minizip)
