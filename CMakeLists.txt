cmake_minimum_required(VERSION 3.10)
project(fileShare VERSION 1.0 LANGUAGES CXX)

# C++ version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
find_package(Boost 1.65 REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif()

find_package(OpenSSL REQUIRED)

# Executable sources
set(SOURCES
    src/main.cpp
    src/connection.cpp
    src/crypto.cpp
)

# Add executable
add_executable(peer ${SOURCES})

# Include paths
target_include_directories(peer PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

target_link_libraries(peer PRIVATE
    ${Boost_LIBRARIES}
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)


# Windows-specific
if(WIN32)
    target_link_libraries(peer PRIVATE wsock32 ws2_32)
endif()
