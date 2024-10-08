cmake_minimum_required(VERSION 3.7)

set(CMAKE_SUPPRESS_REGENERATION true)

# Project settings
project(test_idna LANGUAGES CXX)

include_directories(
  include
)

add_executable(test-idna
  test/test-idna.cpp
  test/idna_lib_upa.cpp
  src/idna.cpp
  src/idna_table.cpp
  src/nfc.cpp
  src/nfc_table.cpp
  src/punycode.cpp
)

add_executable(test-nfc
  test/test-nfc.cpp
  src/nfc.cpp
  src/nfc_table.cpp
)

add_executable(test-utf
  test/test-utf.cpp
)

# Testing

enable_testing()

foreach(test_name test-idna test-nfc test-utf)
  add_test(NAME  ${test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test_name})
endforeach()
