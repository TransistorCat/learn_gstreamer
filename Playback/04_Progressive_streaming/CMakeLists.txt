cmake_minimum_required(VERSION 3.16)

# Target: playback-tutorial-4
add_executable(playback-tutorial-4 playback-tutorial-4.c)

target_compile_options(playback-tutorial-4 PRIVATE -g -O0)

target_link_libraries(playback-tutorial-4 PRIVATE PkgConfig::GST PkgConfig::GST_AUDIO)



