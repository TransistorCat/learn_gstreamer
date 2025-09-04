cmake_minimum_required(VERSION 3.16)

# Target: basic_tutorial_9
add_executable(basic_tutorial_9 basic-tutorial-9.c)

target_compile_options(basic_tutorial_9 PRIVATE -g -O0)

# GStreamer pbutils is required for Discoverer API used in this tutorial
pkg_check_modules(GST_PBUTILS REQUIRED IMPORTED_TARGET gstreamer-pbutils-1.0)

target_link_libraries(basic_tutorial_9 PRIVATE PkgConfig::GST PkgConfig::GST_PBUTILS)



