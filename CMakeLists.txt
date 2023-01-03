cmake_minimum_required(VERSION 3.16.3)

project(cliot)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  message(FATAL_ERROR "GCC 11+ required for building cliot")
endif()

option(BUILD_TESTS "Build tests" TRUE)
option(VERBOSE "Verbose build" TRUE)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
  set(FETCHCONTENT_QUIET FALSE CACHE STRING "Verbose FetchContent()")
endif()

add_executable(cliot)
target_compile_features(cliot PUBLIC cxx_std_20)
target_include_directories(cliot PUBLIC src)

include(FetchContent)
include(ExternalProject)
include(CMake/settings.cmake)
include(CMake/deps/di.cmake)
include(CMake/deps/cxxopts.cmake)
include(CMake/deps/inja.cmake)
include(CMake/deps/libfmt.cmake)
include(CMake/deps/yaml-cpp.cmake)
include(CMake/deps/boost.cmake)

target_sources(cliot PRIVATE
  src/main.cpp
  src/util/parse_uri.cpp
)

if(BUILD_TESTS)
  add_executable(cliot_tests
    unittests/test.cpp
    unittests/web_tests.cpp
  )
  include(CMake/deps/gtest.cmake)
endif()
