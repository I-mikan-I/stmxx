cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cppstm VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_SCAN_FOR_MODULES)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fcolor-diagnostics)

add_library(lib)
target_sources(lib PUBLIC FILE_SET CXX_MODULES FILES ./src/lib.cpp)
# add_executable(main)
# target_sources(main PUBLIC FILE_SET CXX_MODULES FILES ./src/main.cpp)
target_link_libraries(lib atomic)
target_compile_options(lib PRIVATE -O1 -g)

# target_link_libraries(main lib)
# Print all targets defined in the project
# adds test target
add_subdirectory(./test EXCLUDE_FROM_ALL)