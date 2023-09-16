# FindICU requires 3.7
cmake_minimum_required(VERSION 3.7)

set(CMAKE_SUPPRESS_REGENERATION true)

# use ICU_ROOT
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# Project settings
project(test_idna LANGUAGES CXX)

include_directories(
  include
)

find_package(ICU REQUIRED COMPONENTS i18n uc)

add_executable(test-idna
  test/test-idna.cpp
  test/idna_lib_upa.cpp
  src/idna.cpp
  src/idna_table.cpp
  src/normalize.cpp
  src/punycode.cpp
)
target_link_libraries(test-idna ICU::i18n ICU::uc)

# Testing

enable_testing()

add_test(NAME test-idna
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test-idna)
