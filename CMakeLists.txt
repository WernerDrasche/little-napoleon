cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
project(patience)

find_package(SFML 3.0.0 EXACT COMPONENTS Graphics Window System REQUIRED)

if(WIN32)
add_executable(main WIN32 main.cpp)
target_link_libraries(main SFML::Graphics ${CMAKE_SOURCE_DIR}/sfml-main-s.lib)
else()
add_executable(main main.cpp)
target_link_libraries(main SFML::Graphics)
endif()