target_compile_options(lib_cliot  
  PUBLIC -Wall
         -Werror
         -Wextra
         -Wpedantic
         -pedantic
         -Wno-error=unused-parameter
         -Wno-unused-parameter # for now
         -Wno-deprecated-declarations
)
