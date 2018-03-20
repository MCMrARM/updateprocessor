cmake_minimum_required(VERSION 3.10)
project(updateprocesser)

set(CMAKE_CXX_STANDARD 11)

include_directories(playapi/lib/)
add_subdirectory(playapi)
set_target_properties(gplaydl PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_executable(updateprocesser main.cpp play_device.cpp play_device.h play_manager.cpp play_manager.h playapi/src/config.cpp)
target_link_libraries(updateprocesser gplayapi)