

cmake_minimum_required(VERSION 3.12)
project(rim-tarea-1)

set(CMAKE_CXX_STANDARD 11)


find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(rim-tarea-1 main.cpp main.cpp)
target_link_libraries( rim-tarea-1 ${OpenCV_LIBS} )
