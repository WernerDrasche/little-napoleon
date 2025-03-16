cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
project(patience)

find_package(SFML 3.0.0 EXACT COMPONENTS Graphics Window System REQUIRED)

if(WIN32)
add_executable(main WIN32 main.cpp config.cpp)
target_link_libraries(main SFML::Graphics ${CMAKE_SOURCE_DIR}/sfml-main-s.lib)
else()
add_executable(main main.cpp config.cpp)
target_link_libraries(main SFML::Graphics)
endif()
