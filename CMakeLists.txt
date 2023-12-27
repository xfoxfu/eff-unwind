cmake_minimum_required(VERSION 3.14.0)
project(eff-unwind VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.3.2
)

FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
  GIT_TAG         10.1.1
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
  scope_guard
  GIT_REPOSITORY  https://github.com/ricab/scope_guard.git
  GIT_TAG         v1.1.0
)
FetchContent_Populate(scope_guard)
add_library(scope_guard INTERFACE)
target_include_directories(scope_guard INTERFACE ${scope_guard_SOURCE_DIR})

add_library(eff-unwind
  "include/eff-unwind.hpp"
  "src/eff-unwind.cpp"
)
target_compile_features(eff-unwind PUBLIC cxx_std_20)
set_target_properties(eff-unwind PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(eff-unwind PUBLIC include)
target_link_libraries(eff-unwind PUBLIC scope_guard fmt::fmt)
target_compile_definitions(eff-unwind PUBLIC _GNU_SOURCE)

add_executable(tests
  "test/main.cpp"
)
target_compile_features(tests PRIVATE cxx_std_20)
set_target_properties(tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(tests PRIVATE eff-unwind)
