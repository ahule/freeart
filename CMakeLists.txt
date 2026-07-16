cmake_minimum_required(VERSION 3.20)
project(freeart CXX)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

include_directories(include)

set(SOURCE_FILES
        src/main.cpp
        src/server.cpp
        src/fcp.cpp
)

add_executable(freeart ${SOURCE_FILES})

find_package(Threads REQUIRED)
target_link_libraries(freeart PRIVATE Threads::Threads)

add_executable(client client.cpp src/fcp.cpp)
target_link_libraries(client PRIVATE Threads::Threads)