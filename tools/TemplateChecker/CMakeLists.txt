add_executable(TemplateChecker main.cpp)

# specify the libs that the target 'TemplateChecker' needs
# !!! DO NOT change the order of the libs !!!
# libs will be linked in this order, and changing the order could lead to link errors
target_link_libraries(TemplateChecker
  framework
  stack_uaf_checker
  immediate_uaf_checker
  loop_doublefree_checker
  borrowed_reference_checker
  realloc_checker
  memory_alloc_checker
  P2A
  ${CLANG_LIBS}
  ${LLVM_LIBS}
  ${OTHER_LIBS}
)
