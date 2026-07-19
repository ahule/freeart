cmake_minimum_required(VERSION 3.20)
project(freeart CXX)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

include_directories(include)

find_package(Threads REQUIRED)

add_executable(freeart
        main.cpp
        src/server.cpp
        src/fcp.cpp
)
target_link_libraries(freeart PRIVATE Threads::Threads)

add_executable(benchmark
        src/benchmark.cpp
        src/fcp.cpp
)
add_executable(client
        client.cpp
        src/fcp.cpp
)
target_link_libraries(benchmark PRIVATE Threads::Threads)