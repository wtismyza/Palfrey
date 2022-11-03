# System requirements: 

Clang version 12.0 or Clang version 13.0 

# Steps to build Palfrey

1. `mkdir build`

2. `cd build`

3. `CC=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/bin/clang CXX=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/bin/clang++ cmake -DLLVM_PREFIX=pathto/clang+llvm-12.0.0-x86_64-apple-darwin/ -D CMAKE_BUILD_TYPE=Debug ..` **Please check the path to Clang in your system**

4. `make`

# Steps to test your codes (single file)

To make sure the checker is able to test your code, LLVM should generate abstract syntax tree in your code.

1. `cd tests/ReturnStackAddrChecker/test1`

2. `pathto/clang+llvm-12.0.0-x86_64-apple-darwin/bin/clang -emit-ast test.c`

3. `find $(pwd) -name "*.ast" > astList.txt`

4. `pathto/StaticChecker/build/tools/TemplateChecker/TemplateChecker astList.txt ../../config.txt`

# Steps to test a project

1. unzip the source code:
`tar xvf xxx.tar.xz` 

2. enter the directory of the source code:
`cd xxx` 

3. build the project:
`mkdir build-ast` && `cd build-ast` 

4. set up the environment variable: 
`export CC=/path/to/clang`
`export CXX=/path/to/clang++`

5. generate **makefile**: 
    1. for **cmake** project
     `cmake -DCMAKE_BUILD_TYPE=Release ..`
    2. for other cases
     `../configure` 

6. build your program and generate compile_commands.json **(use Bear)**:
`bear -- make`

(compiledb should also work to generate compile_commands.json)

7. copy [getastcmd.py](../benchmark/getastcmd.py) to current directory, run **getastcmd.py** to generate buildast.sh and astList.txt:
    1. `cp /path/to/getastcmd.py .`
    2. `python3 getastcmd.py`

8. run **buildast.sh**:
`chmod +x buildast.sh`
`./buildast.sh | tee buildast.log` # **buildast.log** is used to see if there is error when generating ast

9. analyze **astList.txt**:
`pathto/StaticChecker/build/tools/TemplateChecker/TemplateChecker astList.txt pathto/config.txt`
