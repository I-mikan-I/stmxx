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
target_compile_options(tests PRIVATE -Og -g)

if(SANITIZE)
target_compile_definitions(lib PRIVATE THREAD_SANITIZER=1)
target_compile_definitions(tests PRIVATE THREAD_SANITIZER=1)
target_compile_options(tests PRIVATE -fsanitize=thread)
target_link_libraries(tests PRIVATE tsan)
else()
target_compile_definitions(lib PRIVATE THREAD_SANITIZER=0)
target_compile_definitions(tests PRIVATE THREAD_SANITIZER=0)
endif()