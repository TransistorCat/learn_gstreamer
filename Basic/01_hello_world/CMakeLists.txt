cmake_minimum_required(VERSION 3.16)

# Target: hello_gst
add_executable(basic-tutorial-1 basic-tutorial-1.c)

target_compile_options(basic-tutorial-1 PRIVATE -g -O0)

target_link_libraries(basic-tutorial-1 PRIVATE PkgConfig::GST)


