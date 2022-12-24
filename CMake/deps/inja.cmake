FetchContent_Declare(
  inja
  URL https://github.com/pantor/inja/archive/refs/tags/v3.3.0.zip
)

FetchContent_GetProperties(inja)

if(NOT inja_POPULATED)
  FetchContent_Populate(inja)
  add_subdirectory(${inja_SOURCE_DIR} ${inja_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(cliot PRIVATE inja)
