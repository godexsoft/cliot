FetchContent_Declare(
  cxxopts
  URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.zip
)

FetchContent_GetProperties(cxxopts)

if(NOT cxxopts_POPULATED)
  FetchContent_Populate(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(lib_cliot PUBLIC cxxopts)
