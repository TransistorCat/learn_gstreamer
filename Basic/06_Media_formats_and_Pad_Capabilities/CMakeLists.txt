cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_6
add_executable(basic_tutorial_6 basic-tutorial-6.c)

target_compile_options(basic_tutorial_6 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_6 PRIVATE PkgConfig::GST)



