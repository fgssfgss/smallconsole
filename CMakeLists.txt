cmake_minimum_required(VERSION 3.15)
project(smallconsole C)

if (EMSCRIPTEN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -r -sUSE_SDL=2 -O2")
    set(CMAKE_EXECUTABLE_SUFFIX ".bc")
else()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${smallconsole_SOURCE_DIR}/cmake")
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_FLAGS "-O1 -pipe -g -fsanitize=address -fno-omit-frame-pointer")
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIR})
endif()

add_executable(smallconsole main.c
                            common.c
                            cpu.c
                            gpu.c
                            joypad.c
                            rom.c
                            norom.c
                            mbc1.c
                            timer.c)

if (EMSCRIPTEN)
    add_custom_command(TARGET smallconsole
    COMMAND ${CMAKE_C_COMPILER} -O2 -o index.html ${CMAKE_CURRENT_BINARY_DIR}/smallconsole.bc -sFORCE_FILESYSTEM=1 -sUSE_SDL=2 --shell-file "${smallconsole_SOURCE_DIR}/emscripten_page.html"
    POST_BUILD
    COMMENT "Creating HTML file, please copy index.* files to your server root dir")
else()
    target_link_libraries(smallconsole "${SDL2_LIBRARY}")
endif()
