add_library(freeart_core STATIC src/utils.cpp)

target_include_directories(freeart_core PUBLIC include)

target_link_libraries(freeart_core PUBLIC Threads::Threads)

target_compile_features(freeart_core PUBLIC cxx_std_20)
