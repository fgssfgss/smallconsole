cmake_minimum_required(VERSION 3.15)
project(smallconsole C)

set(CMAKE_C_STANDARD 11)

add_executable(smallconsole main.c common.c cpu.c)