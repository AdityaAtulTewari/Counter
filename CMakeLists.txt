cmake_minimum_required(VERSION 3.10)
project(Counter)

set(CMAKE_CXX_STANDARD 17)

find_package( Threads )


add_executable(Counter src/main.cpp src/counter.h src/timing.h)

target_link_libraries(Counter ${CMAKE_THREAD_LIBS_INIT})
