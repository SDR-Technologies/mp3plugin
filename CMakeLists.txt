cmake_minimum_required(VERSION 3.1)
project(mp3filewriter
    VERSION 1.0
    DESCRIPTION "mp3filewriter plugin Cmake"
    LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Release)
include_directories(.)

set(SOURCES "")
list(APPEND SOURCES
    iqaccumulator.cpp
    mp3filewriter.cpp
    mp3writer.cpp)

add_library(mp3filewriter SHARED ${SOURCES})
target_link_libraries(mp3filewriter pthread)
target_link_libraries(mp3filewriter mp3lame)
target_link_libraries(mp3filewriter samplerate)


set(CMAKE_CXX_FLAGS_RELEASE "-O2")
# Install library
install(TARGETS ${PROJECT_NAME} DESTINATION /opt/vmbase/extensions )
