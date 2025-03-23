cmake_minimum_required(VERSION 3.7...3.13)

set(CMAKE_SUPPRESS_REGENERATION true)

# Extract version from idna_version.h
file(READ include/upa/idna/idna_version.h upa_idna_version_h)
if (NOT upa_idna_version_h MATCHES "[ \t]+UPA_IDNA_VERSION[ \t]+\"([0-9]+(\.[0-9]+)+)\"")
  message(FATAL_ERROR "Cannot get UPA_IDNA_VERSION from idna_version.h")
endif()
set(UPA_IDNA_VERSION ${CMAKE_MATCH_1})
message(STATUS "Upa IDNA version: ${UPA_IDNA_VERSION}")

# Project settings
project(test_idna VERSION ${UPA_IDNA_VERSION} LANGUAGES CXX)

if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

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
