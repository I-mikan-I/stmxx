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
target_compile_options(tests PRIVATE -O1 -g)

if(SANITIZE_THREAD)
target_compile_definitions(lib PRIVATE THREAD_SANITIZER=1)
target_compile_definitions(tests PRIVATE THREAD_SANITIZER=1)
target_compile_options(tests PRIVATE -fsanitize=thread)
target_link_libraries(tests PRIVATE tsan)
elseif(SANITIZE_ADDRESS)
target_compile_definitions(tests PRIVATE THREAD_SANITIZER=1)
target_compile_options(tests PRIVATE -fsanitize=address)
target_link_libraries(tests PRIVATE asan)
target_compile_options(tests PRIVATE -O2)
target_compile_options(lib PRIVATE -O2)
else()
target_compile_definitions(lib PRIVATE THREAD_SANITIZER=0)
target_compile_definitions(tests PRIVATE THREAD_SANITIZER=0)
endif()