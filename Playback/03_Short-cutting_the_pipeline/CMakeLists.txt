cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-3
add_executable(playback-tutorial-3 playback-tutorial-3.c)

target_compile_options(playback-tutorial-3 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-3 PRIVATE PkgConfig::GST PkgConfig::GST_AUDIO)



