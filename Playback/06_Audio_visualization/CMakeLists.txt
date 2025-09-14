cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-6
add_executable(playback-tutorial-6 playback-tutorial-6.c)

target_compile_options(playback-tutorial-5 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-6 PRIVATE PkgConfig::GST PkgConfig::GST_AUDIO)
