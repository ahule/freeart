add_executable(freeart_chat
    server_main.cpp
    src/fcp.cpp
    src/chat_handler.cpp
)
target_include_directories(freeart_chat PRIVATE include)
target_link_libraries(freeart_chat PRIVATE freeart_core)

add_executable(freeart_client
    client.cpp
    src/fcp.cpp
)
target_include_directories(freeart_client PRIVATE include)
target_link_libraries(freeart_client PRIVATE freeart_core)
