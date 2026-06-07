cmake_minimum_required(VERSION 4.2)
project(freeart)

set(CMAKE_CXX_STANDARD 20)

add_executable(freeart main.cpp
        labs.cpp
        client.cpp
        fcp.h
        clab.cpp
        clab.cpp
)
