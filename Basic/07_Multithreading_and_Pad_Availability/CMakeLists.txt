cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_7
add_executable(basic_tutorial_7 basic-tutorial-7.c)

target_compile_options(basic_tutorial_7 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_7 PRIVATE PkgConfig::GST)



