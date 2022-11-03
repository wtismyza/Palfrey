add_subdirectory(stack_uaf_checker)
add_subdirectory(immediate_uaf_checker)
add_subdirectory(loop_doublefree_checker)
add_subdirectory(borrowed_reference_checker)
add_subdirectory(realloc_checker)
add_subdirectory(memory_alloc_checker)

#aux_source_directory(. checkers)
#add_library(checkers ${checkers})