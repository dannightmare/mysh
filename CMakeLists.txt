cmake_minimum_required(VERSION 3.10)
project(my_shell)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)

add_executable(mysh
    src/main.cpp
)
