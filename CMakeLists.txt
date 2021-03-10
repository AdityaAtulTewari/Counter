cmake_minimum_required(VERSION 3.10)
project(Counter)

set(CMAKE_CXX_STANDARD 17)

find_package( Threads )

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)

add_executable(Counter src/main.cpp src/counter.h src/timing.h)


if(NOGEM5)
  set(CMAKE_CXX_FLAGS "-DNOGEM5=true ${CMAKE_CXX_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/benchmarks/VirtualLink/near-data-sim/util/")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/benchmarks/VirtualLink/near-data-sim/util/m5")
  target_link_libraries(Counter libm5.a)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/benchmarks/VirtualLink/libvl")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/benchmarks/VirtualLink/libvl/libvl/")
endif()

target_link_libraries(Counter ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(Counter libvl.a)
