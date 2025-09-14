cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-7
add_executable(playback-tutorial-7 playback-tutorial-7.c)

target_compile_options(playback-tutorial-7 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-7 PRIVATE PkgConfig::GST PkgConfig::GST_AUDIO)
