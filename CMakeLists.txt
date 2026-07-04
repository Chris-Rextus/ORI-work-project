cmake_minimum_required(VERSION 3.20)
project(ori_trabalho CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)          

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ori GrupoPYP.cpp)
target_link_libraries(ori PRIVATE Qt6::Widgets)
target_compile_options(ori PRIVATE -Wall -Wextra)