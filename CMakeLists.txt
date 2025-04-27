cmake_minimum_required(VERSION 3.7...3.13)

set(CMAKE_SUPPRESS_REGENERATION true)

# Extract version from idna_version.h
file(READ include/upa/idna/idna_version.h upa_idna_version_h)
if (NOT upa_idna_version_h MATCHES "[ \t]+UPA_IDNA_VERSION[ \t]+\"([0-9]+(\.[0-9]+)+)\"")
  message(FATAL_ERROR "Cannot get UPA_IDNA_VERSION from idna_version.h")
endif()
set(UPA_IDNA_VERSION ${CMAKE_MATCH_1})
message(STATUS "Upa IDNA version: ${UPA_IDNA_VERSION}")
# Create SOVERSION from major and minor version numbers
string(REGEX MATCH "^[0-9]+\.[0-9]+" UPA_IDNA_SOVERSION ${UPA_IDNA_VERSION})
message(STATUS "Upa IDNA SOVERSION: ${UPA_IDNA_SOVERSION}")

# Project settings
project(test_idna VERSION ${UPA_IDNA_VERSION} LANGUAGES CXX)

if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Library settings

add_library(upa_idna
  src/idna.cpp
  src/idna_table.cpp
  src/nfc.cpp
  src/nfc_table.cpp
  src/punycode.cpp)
target_include_directories(upa_idna PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
if (BUILD_SHARED_LIBS)
  target_compile_definitions(upa_idna PRIVATE UPA_LIB_EXPORT
    INTERFACE UPA_LIB_IMPORT)
endif()
set_target_properties(upa_idna PROPERTIES
  VERSION ${UPA_IDNA_VERSION}
  SOVERSION ${UPA_IDNA_SOVERSION}
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

# Test targets

add_executable(test-idna
  test/test-idna.cpp
  test/idna_lib_upa.cpp)
target_link_libraries(test-idna PRIVATE upa_idna)

add_executable(test-nfc
  test/test-nfc.cpp)
target_link_libraries(test-nfc PRIVATE upa_idna)

add_executable(test-utf
  test/test-utf.cpp)
target_link_libraries(test-utf PRIVATE upa_idna)

# Testing

enable_testing()

foreach(test_name test-idna test-nfc test-utf)
  add_test(NAME  ${test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test_name})
endforeach()
