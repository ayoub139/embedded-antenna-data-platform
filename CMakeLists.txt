cmake_minimum_required(VERSION 3.16)

project(AntennaSimulator LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(client 
    client/client.c 
    client/antenna_simulator.c
)

add_executable(server 
    server/server.c
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)

target_compile_options(client PRIVATE -Wall -Wextra)
target_compile_options(server PRIVATE -Wall -Wextra)

