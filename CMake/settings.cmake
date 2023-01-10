target_compile_options(lib_cliot  
  PUBLIC -Wall
         -Werror
         -Wextra
         #-Wshadow
         #-Wnon-virtual-dtor
         -Wpedantic
         -pedantic
         -Wno-error=unused-parameter
         #-Wno-error=shadow
         #-Wno-error=non-virtual-dtor
         -Wno-unused-parameter # for now
         -Wno-deprecated-declarations
)
