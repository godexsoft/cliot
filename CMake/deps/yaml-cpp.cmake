FetchContent_Declare(
  yamlcpp
  URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip
)

FetchContent_GetProperties(yamlcpp)

if(NOT yamlcpp_POPULATED)
  FetchContent_Populate(yamlcpp)
  add_subdirectory(${yamlcpp_SOURCE_DIR} ${yamlcpp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(lib_cliot PUBLIC yaml-cpp)
