cmake_minimum_required(VERSION 3.15.1)

project(remuxing LANGUAGES C)
find_package(FFMPEG REQUIRED)

message(CHECK: ${FFMPEG_FOUND})
message(CHECK2: ${FFMPEG_FIND_COMPONENTS})
set(SOURCE_FILES src/remuxing.c)

add_executable(remuxing ${SOURCE_FILES})
#target_include_directories(remuxing PRIVATE ${FFMPEG_INCLUDE_DIRS})
#target_link_libraries(remuxing PRIVATE ${FFMPEG_LIBRARIES})