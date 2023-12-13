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

if (APPLE)
  include(Addbrew)
  add_brew_prefix(llvm)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${BREW_llvm_PREFIX})
endif ()
find_package(llvm REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
add_library(llvm_libunwind INTERFACE IMPORTED)
target_include_directories(llvm_libunwind INTERFACE ${LLVM_INCLUDE_DIRS})
# target_link_directories(llvm_libunwind INTERFACE ${LLVM_LIBRARY_DIRS})
# target_link_libraries(llvm_libunwind INTERFACE unwind)
add_library(llvm::libunwind ALIAS llvm_libunwind)

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
target_link_libraries(tests PRIVATE scope_guard fmt::fmt)
