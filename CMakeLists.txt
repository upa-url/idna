# FindICU requires 3.7
cmake_minimum_required(VERSION 3.7)

set(CMAKE_SUPPRESS_REGENERATION true)

# use ICU_ROOT
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# Project settings
project(idna_test LANGUAGES CXX)

find_package(ICU REQUIRED COMPONENTS i18n uc)

add_executable(idna_test idna_test.cpp test_icu.cpp)
target_link_libraries(idna_test ICU::i18n ICU::uc)

# Testing

enable_testing()

add_test(NAME idna_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/idna_test)
