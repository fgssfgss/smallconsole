cmake_minimum_required(VERSION 3.15)
project(smallconsole C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${smallconsole_SOURCE_DIR}/cmake")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O2 -pipe")
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(smallconsole main.c common.c cpu.c gpu.c joypad.c rom.c sound.c)
target_link_libraries(smallconsole "${SDL2_LIBRARY}")
