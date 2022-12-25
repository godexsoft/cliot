target_compile_options(cliot  
  PRIVATE -Wall
          -Werror
          -Wextra
          -Wpedantic
          -pedantic
          -Wno-error=unused-parameter
          -Wno-unused-parameter # for now
)
