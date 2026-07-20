cmake_minimum_required(VERSION 3.20)
project(freeart CXX)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

add_subdirectory(core)
add_subdirectory(apps/chat)
add_subdirectory(examples/echo)