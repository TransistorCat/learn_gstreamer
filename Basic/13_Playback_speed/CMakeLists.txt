cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_8
add_executable(basic_tutorial_13 basic-tutorial-13.c)

target_compile_options(basic_tutorial_13 PRIVATE -g -O0)

# Link both gstreamer-1.0 and gstreamer-audio-1.0 libraries
target_link_libraries(basic_tutorial_13 PRIVATE PkgConfig::GST)



