cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_2
add_executable(basic_tutorial_2 basic-tutorial-2.c)

target_compile_options(basic_tutorial_2 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_2 PRIVATE PkgConfig::GST)


