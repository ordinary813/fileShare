cmake_minimum_required(VERSION 3.10)
project(fileShare VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.65 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(PEER_SOURCES
    src/connection.cpp
    src/crypto.cpp
    src/peer_main.cpp
)

add_executable(peer ${PEER_SOURCES})
target_include_directories(peer PRIVATE ${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(peer PRIVATE ${Boost_LIBRARIES} OpenSSL::Crypto nlohmann_json::nlohmann_json)

add_executable(tracker src/tracker.cpp)
target_include_directories(tracker PRIVATE ${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(tracker PRIVATE ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
