cmake_minimum_required(VERSION 3.30)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests)
target_sources(tests PRIVATE FILE_SET CXX_MODULES FILES ./test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain lib)