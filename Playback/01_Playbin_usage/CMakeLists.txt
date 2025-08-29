cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-1
add_executable(playback-tutorial-1 playback-tutorial-1.c)

target_compile_options(playback-tutorial-1 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-1 PRIVATE PkgConfig::GST)



