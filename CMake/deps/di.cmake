FetchContent_Declare(
  di
  GIT_REPOSITORY https://github.com/godexsoft/di.git
  GIT_TAG        feature/lazy
)

FetchContent_GetProperties(di)

if(NOT di_POPULATED)
  FetchContent_Populate(di)
  add_subdirectory(${di_SOURCE_DIR} ${di_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(lib_cliot PUBLIC di)
