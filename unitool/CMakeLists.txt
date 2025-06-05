cmake_minimum_required(VERSION 3.7...3.13)

# Settings
set(CMAKE_SUPPRESS_REGENERATION true)

# Project settings
project(unitool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(unitool-idna unitool-idna.cpp)
add_executable(unitool-nfc unitool-nfc.cpp)
