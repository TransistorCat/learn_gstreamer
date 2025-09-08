cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-2
add_executable(playback-tutorial-2 playback-tutorial-2.c)

target_compile_options(playback-tutorial-2 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-2 PRIVATE PkgConfig::GST)



