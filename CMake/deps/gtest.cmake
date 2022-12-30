FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_GetProperties(googletest)

if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_compile_features(cliot_tests PUBLIC cxx_std_20)
target_link_libraries(cliot_tests PUBLIC gmock_main inja fmt di yaml-cpp ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
target_include_directories(cliot_tests PRIVATE unittests src ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

enable_testing()

include(GoogleTest)

gtest_discover_tests(cliot_tests)
