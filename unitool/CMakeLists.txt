cmake_minimum_required(VERSION 3.7)

# Settings
set(CMAKE_SUPPRESS_REGENERATION true)

# Project settings
project(unitool LANGUAGES CXX)

add_executable(unitool-idna unitool-idna.cpp)
