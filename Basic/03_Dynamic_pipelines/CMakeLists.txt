cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_3
add_executable(basic_tutorial_3 basic-tutorial-3.c)

target_compile_options(basic_tutorial_3 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_3 PRIVATE PkgConfig::GST)


