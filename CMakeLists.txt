cmake_minimum_required(VERSION 3.7)
project(updateprocessor)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DMSA_LOG_NETWORK)

include_directories(playapi/lib/)
add_subdirectory(playapi)
add_subdirectory(base64)
add_subdirectory(logger)
add_subdirectory(rapidxml)
add_subdirectory(msa)
set_target_properties(gplaydl PROPERTIES EXCLUDE_FROM_ALL TRUE)

set(WEBSOCKET_LIB_PATH uWebSockets)
set(WEBSOCKET_LIB_SOURCES ${WEBSOCKET_LIB_PATH}/src/Extensions.cpp ${WEBSOCKET_LIB_PATH}/src/Group.cpp ${WEBSOCKET_LIB_PATH}/src/Networking.cpp ${WEBSOCKET_LIB_PATH}/src/Hub.cpp ${WEBSOCKET_LIB_PATH}/src/Node.cpp ${WEBSOCKET_LIB_PATH}/src/WebSocket.cpp ${WEBSOCKET_LIB_PATH}/src/HTTPSocket.cpp ${WEBSOCKET_LIB_PATH}/src/Socket.cpp ${WEBSOCKET_LIB_PATH}/src/Epoll.cpp win10_store_network.cpp win10_store_network.h win10_store_manager.cpp win10_store_manager.h)
include_directories(${WEBSOCKET_LIB_PATH}/src)

include_directories(json/include)

add_executable(updateprocessor ${WEBSOCKET_LIB_SOURCES} main.cpp play_device.cpp play_device.h play_manager.cpp play_manager.h playapi/src/config.cpp discord.cpp discord.h discord_gateway.cpp discord_gateway.h discord_state.cpp discord_state.h file_utils.cpp file_utils.h apk_manager.cpp apk_manager.h apk_uploader.cpp apk_uploader.h wake_on_lan.cpp wake_on_lan.h telegram.cpp telegram.h telegram_state.cpp telegram_state.h)
target_link_libraries(updateprocessor gplayapi rapidxml msa)