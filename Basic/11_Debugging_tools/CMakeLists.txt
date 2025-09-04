cmake_minimum_required(VERSION 3.10)

# Set the project name
add_executable(dot-example dot-example.c)
# Find the GStreamer package and its components
# This requires you to have the GStreamer development files installed
# (e.g., libgstreamer1.0-dev on Debian/Ubuntu)

# Add the source file to create an executable
target_compile_options(dot-example PRIVATE -g -O0)

# Link both gstreamer-1.0 and gstreamer-audio-1.0 libraries
target_link_libraries(dot-example PRIVATE PkgConfig::GST)
