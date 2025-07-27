cmake_minimum_required(VERSION 3.10)
project(fileShare VERSION 1.0 LANGUAGES CXX)

# C++ version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package(Boost 1.65 REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif()

# Executable sources
set(SOURCES
    src/main.cpp
    src/connection.cpp
)

# Add executable
add_executable(peer ${SOURCES})

# Include paths
target_include_directories(peer PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# Link Boost
target_link_libraries(peer PRIVATE ${Boost_LIBRARIES})

# Windows-specific
if(WIN32)
    target_link_libraries(peer PRIVATE wsock32 ws2_32)
endif()
