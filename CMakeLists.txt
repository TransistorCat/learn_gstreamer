cmake_minimum_required(VERSION 3.16)

project(learn_gstreamer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use PkgConfig to locate GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GST_AUDIO REQUIRED IMPORTED_TARGET gstreamer-audio-1.0)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(Basic/01_hello_world)
add_subdirectory(Basic/02_GStreamer_concepts)
add_subdirectory(Basic/03_Dynamic_pipelines)
add_subdirectory(Basic/04_Time_management)
add_subdirectory(Basic/05_GUI_toolkit_integration)

add_subdirectory(Basic/06_Media_formats_and_Pad_Capabilities)
add_subdirectory(Basic/07_Multithreading_and_Pad_Availability)
add_subdirectory(Basic/08_Short-cutting_the_pipeline)
add_subdirectory(Basic/09_Media_information_gathering)
add_subdirectory(Basic/11_Debugging_tools)
add_subdirectory(Basic/12_Streaming)
add_subdirectory(Basic/13_Playback_speed)
add_subdirectory(Playback/01_Playbin_usage)
add_subdirectory(Playback/02_Subtitle_management)