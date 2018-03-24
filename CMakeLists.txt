cmake_minimum_required(VERSION 3.10)
project(updateprocesser)

set(CMAKE_CXX_STANDARD 11)

include_directories(playapi/lib/)
add_subdirectory(playapi)
set_target_properties(gplaydl PROPERTIES EXCLUDE_FROM_ALL TRUE)

set(WEBSOCKET_LIB_PATH uWebSockets)
set(WEBSOCKET_LIB_SOURCES ${WEBSOCKET_LIB_PATH}/src/Extensions.cpp ${WEBSOCKET_LIB_PATH}/src/Group.cpp ${WEBSOCKET_LIB_PATH}/src/Networking.cpp ${WEBSOCKET_LIB_PATH}/src/Hub.cpp ${WEBSOCKET_LIB_PATH}/src/Node.cpp ${WEBSOCKET_LIB_PATH}/src/WebSocket.cpp ${WEBSOCKET_LIB_PATH}/src/HTTPSocket.cpp ${WEBSOCKET_LIB_PATH}/src/Socket.cpp ${WEBSOCKET_LIB_PATH}/src/Epoll.cpp discord.cpp discord.h discord_gateway.cpp discord_gateway.h discord_state.cpp discord_state.h)
include_directories(${WEBSOCKET_LIB_PATH}/src)

include_directories(json/include)

add_executable(updateprocesser ${WEBSOCKET_LIB_SOURCES} main.cpp play_device.cpp play_device.h play_manager.cpp play_manager.h playapi/src/config.cpp)
target_link_libraries(updateprocesser gplayapi)