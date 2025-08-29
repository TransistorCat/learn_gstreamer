cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_4
add_executable(basic_tutorial_4 basic-tutorial-4.c)

target_compile_options(basic_tutorial_4 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_4 PRIVATE PkgConfig::GST)



