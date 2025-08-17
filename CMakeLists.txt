cmake_minimum_required(VERSION 3.10)
project(fileShare VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0167) # CMake >= 3.31
    cmake_policy(SET CMP0167 NEW)
    find_package(Boost 1.65 CONFIG REQUIRED COMPONENTS system)
else()
    find_package(Boost 1.65 REQUIRED COMPONENTS system)
endif()

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(PEER_SOURCES
    src/connection.cpp
    src/crypto.cpp
    src/peer_main.cpp
)

set(TRACKER_SOURCES
    src/tracker.cpp
    src/connection.cpp
)

add_executable(peer ${PEER_SOURCES})
target_include_directories(peer PRIVATE ${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(peer PRIVATE ${Boost_LIBRARIES} OpenSSL::Crypto nlohmann_json::nlohmann_json)

add_executable(tracker ${TRACKER_SOURCES})
target_include_directories(tracker PRIVATE ${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(tracker PRIVATE ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

# Post-build commands to copy into peer0 and peer1
add_custom_command(TARGET peer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/peer0
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/peer0/shared_files

    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/peer1
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/peer1/shared_files

    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:peer> ${CMAKE_SOURCE_DIR}/peer0/peer
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:peer> ${CMAKE_SOURCE_DIR}/peer1/peer
)
