cmake_minimum_required(VERSION 3.10)
project(Counter)

set(CMAKE_CXX_STANDARD 17)

find_package( Threads )
find_package( Boost )

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)

add_executable(Counter src/main.cpp src/counter.h src/timing.h)

if(NOGEM5)
  set(CMAKE_CXX_FLAGS "-DNOGEM5=true ${CMAKE_CXX_FLAGS}")
  target_link_libraries(Counter libvl.a)
else()
  target_include_directories(Counter PRIVATE "/benchmarks/VirtualLink/near-data-sim/include/")
  target_link_libraries(Counter /benchmarks/VirtualLink/near-data-sim/util/m5/libm5.a)
  target_include_directories(Counter PRIVATE "/benchmarks/VirtualLink/libvl")
  target_link_libraries(Counter /benchmarks/VirtualLink/libvl/libvl/libvl.a)
  set(CMAKE_CXX_FLAGS "-DVL ${CMAKE_CXX_FLAGS}")
endif()

if(Boost_FOUND)
  set(CMAKE_CXX_FLAGS "-DBOOST ${CMAKE_CXX_FLAGS}")
  include_directories(${Boost_INCLUDE_DIRS})
endif()

if(EA)
  set(CMAKE_CXX_FLAGS "-DEA ${CMAKE_CXX_FLAGS}")
  target_include_directories(Counter PRIVATE "repos/MPMCQueue/include")
endif()

target_link_libraries(Counter ${CMAKE_THREAD_LIBS_INIT})
