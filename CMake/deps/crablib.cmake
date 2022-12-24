FetchContent_Declare(
  crablib
  URL https://github.com/hrissan/crablib/archive/refs/tags/0.9.1.zip
)

FetchContent_GetProperties(crablib)

if(NOT crablib_POPULATED)
  FetchContent_Populate(crablib)
  add_subdirectory(${crablib_SOURCE_DIR} ${crablib_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(cliot PRIVATE crablib::crablib)
