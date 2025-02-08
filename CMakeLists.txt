cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cppstm VERSION 1.0 LANGUAGES CXX)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_SCAN_FOR_MODULES)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fcolor-diagnostics)

add_library(lib)
target_sources(lib PUBLIC FILE_SET CXX_MODULES FILES ./src/lib.cpp)
add_executable(cppstm)
target_sources(cppstm PUBLIC FILE_SET CXX_MODULES FILES ./src/main.cpp)
target_link_libraries(lib atomic)

target_link_libraries(cppstm lib)
# Print all targets defined in the project