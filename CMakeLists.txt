cmake_minimum_required(VERSION 3.14.0)
project(eff-unwind VERSION 0.1.0 LANGUAGES C CXX)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.3.2
)

FetchContent_MakeAvailable(Catch2)

add_library(eff-unwind INTERFACE
  "include/eff-unwind.hpp"
)
target_compile_features(eff-unwind INTERFACE cxx_std_20)
set_target_properties(eff-unwind PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(eff-unwind INTERFACE include)

add_executable(tests
  "test/main.cpp"
)
target_compile_features(tests PRIVATE cxx_std_20)
set_target_properties(tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain eff-unwind)
