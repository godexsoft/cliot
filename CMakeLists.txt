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

add_library(lib_cliot)
add_executable(cliot)
target_compile_features(lib_cliot PUBLIC cxx_std_20)
target_include_directories(lib_cliot PUBLIC src)

include(FetchContent)
include(ExternalProject)
include(CMake/settings.cmake)
include(CMake/deps/di.cmake)
include(CMake/deps/cxxopts.cmake)
include(CMake/deps/inja.cmake)
include(CMake/deps/libfmt.cmake)
include(CMake/deps/yaml-cpp.cmake)
include(CMake/deps/boost.cmake)

target_sources(lib_cliot PUBLIC
  src/web/web_socket_session.cpp
  src/web/async_connection_pool.cpp
  src/web/fetcher.cpp
  src/reporting/default_report_renderer.cpp
  src/validation/validator.cpp
  src/util/parse_uri.cpp
)

target_sources(cliot PRIVATE src/main.cpp)
target_link_libraries(cliot PRIVATE lib_cliot)

if(BUILD_TESTS)
  add_executable(cliot_tests
    unittests/test.cpp
    unittests/web_tests.cpp
  )
  target_link_libraries(cliot_tests PRIVATE lib_cliot)
  target_include_directories(cliot_tests PRIVATE unittests)
  include(CMake/deps/gtest.cmake)
endif()
