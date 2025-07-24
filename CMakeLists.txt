cmake_minimum_required(VERSION 3.10)
project(fileShare VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost
find_package(Boost 1.65 REQUIRED COMPONENTS system)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

# Add executable
add_executable(peer src/main.cpp)

# Link libraries
target_link_libraries(peer ${Boost_LIBRARIES})

# Platform-specific libraries
if(WIN32)
    target_link_libraries(peer wsock32 ws2_32)
endif()