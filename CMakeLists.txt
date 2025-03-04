cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
project(patience)

find_package(SFML 3.0.0 EXACT COMPONENTS Graphics Window System REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main SFML::Graphics)
