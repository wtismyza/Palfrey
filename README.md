Introduction about Palfrey
------------------
Our tool includes a framework for Abstract Syntax Tree (AST) and Control Flow Graph (CFG) analysis, a module for pointer alias analysis, and a module for pattern analysis. Our checkers are mainly based on the AST, call graphs, and CFG of programs. First, the AST is generated from the source code of a program, and then the call graphs and CFGs are constructed based on the AST. Next, after the CFG, AST, and call graphs are constructed, a pattern matching engine is used to identify if UaF bugs exist. The inter-procedure analysis is performed to determine if a memory free operation happens, while the AST analysis is used to decide if specific functions are called. Subsequently, the data flow analysis is performed locally for the alias analysis, and the pattern matching engine is eventually used to report UaF.

1.  It contains seven checkers right now, and the checkers work in both C and C++ program:

    1. `stack_uaf_checker`: gives a warning when the function returns an unsafe stack address. 
    2. `immediate_uaf_checker`: checks if UaF happens within the same and adjacent basic block. The checker locates `free()` or `delete`, and see if the freed memory was read or write before the pointer that points to the freed memory is assigned to an new valid memory. 
    3. `loop_doublefree_checker`: checks repeated memory free in the loop.
    4. `realloc_checker`: checks if size == 0 is called when calling `realloc(p, size)`. (minor adjustment to report `resize(0)`, too many FP in `resize(0)` though.)
    5. `borrowed_reference_checker`: reports unsafe borrowed reference errors in Cpython.
    6. `memory_alloc_checker`: detect `malloc(0)` and `calloc(0, size_t size)`. `malloc(0)` could be problematic when it returns a valid address. This checker is recommended off in [config.txt](./tests/config.txt). 
    7. other API errors: those APIs are application-specific, for example, [array.fromstring in CPython](https://bugs.python.org/issue24613) and [PySlice_GetIndicesEx in CPython](https://bugs.python.org/issue27867). (this checker may not be useful for general purpose, and it will be uploaded with its benchmark code.)
    
2.  Other functions:

    1. the detection depth of `immediate_uaf_checker` is tunable, and by dedault its `checkDepth = 2`, meaning the detection range is in the same or adjacent basic block of the memory free. Adding `checkDepth` in [config.txt](./tests/config.txt) would lead to more possible UaF reports but also expose to more false positives. 
    2. the API of `borrowed_reference_checker` could be reconfigured in [config.txt](./tests/config.txt). 
    3. Point-to analysis: this function helps find more UaF but inaccurate point-to analysis would also lead to more false positives.

**About memory allocation function:**

`realloc()` can exhibit three behaviors from the perspective of function callers: (1) simply expand the original memory block "in place"; (2) the new memory is failed to allocate and the old memory is returned; or (3) allocate a new memory block, copy data, and free the original one. Particularly, `realloc(0)` implies a memory free and can potentially cause dangling pointers. 

`malloc(0)`: If size is zero in [void* malloc (size_t size)](https://cplusplus.com/reference/cstdlib/malloc/?kw=malloc), **the return value depends on the particular library implementation** (it may or may not be a null pointer), but the returned pointer shall not be dereferenced. If size is below zero, the function fails to allocate the requested block of memory and a null pointer is returned. 


`resize(0)`: free the whole vector.

`calloc(0, size_t size)`: `void* calloc( size_t num, size_t size )` If size is zero, the behavior is implementation defined (null pointer may be returned, or some non-null pointer may be returned that may not be used to access storage) 



Build and Test Instructions
------------------

**Steps to build Palfrey**

1. `mkdir build`
2. `cd build`
3. `CC=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/bin/clang CXX=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/bin/clang++ cmake -DLLVM_PREFIX=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/ -D CMAKE_BUILD_TYPE=Debug ..` **Please check the path to Clang in your system**
4. `make`

For test instructions, please check [cmd.md](./doc/cmd.md)

Configuration of the tool
------------------
1. **CheckerEnable** is to decide which checker is on during the analysis.
2. **PrintLog** generates the bug imformation, and by default its level is 5 (level 1 generates more information). 
3. **checkDepth** determines the depth of `immediate_uaf_checker`. 
4. **API** for `borrowed_reference_checker` could be manually tuned.

For more information, please check [config.txt](./tests/config.txt)

Introduction about the framework and InterProcedureCFG
------------------
In term of the **framework** about Palfrey, please check [Framework.md](./doc/Framework.md). You can further take advantage of the `BasicChecker` to design your own checker. 

**InterProcedureCFG** is introduced in [README.md](./include/CFG/README.md)

About Point-to/alias analysis 
------------------
**Point-to/alias analysis** is introduced in [README.md](./include/P2A/README.md). You can take advantage of this module to conduct further memory analysis. 
