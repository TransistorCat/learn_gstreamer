cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_5
add_executable(basic_tutorial_5 basic-tutorial-5.c)

target_compile_options(basic_tutorial_5 PRIVATE -g -O0)

target_link_libraries(basic_tutorial_5 PRIVATE PkgConfig::GST)
target_link_libraries(basic_tutorial_5 PRIVATE PkgConfig::GTK)



