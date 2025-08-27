cmake_minimum_required(VERSION 3.16)

# Target: hello_gst
add_executable(hello_gst basic-tutorial-1.c)

target_compile_options(hello_gst PRIVATE -g -O0)

target_link_libraries(hello_gst PRIVATE PkgConfig::GST)


