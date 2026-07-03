cmake_minimum_required(VERSION 3.20)
project(ori_trabalho C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ori src/main.c)
target_compile_options(ori PRIVATE -Wall -Wextra)