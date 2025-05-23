cmake_minimum_required(VERSION 3.14.0)
project(eff-unwind VERSION 0.1.0 LANGUAGES C CXX)
cmake_policy(SET CMP0167 NEW)

option(EFF_UNWIND_TRACE "Include trace print to stdout" OFF)
option(EFF_UNWIND_ASAN "Build binary with address santizer" OFF)
option(EFF_UNWIND_BUILD_CPP_EFFECTS "Build cpp-effects test cases" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

Include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
  GIT_TAG         11.0.2
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
  scope_guard
  GIT_REPOSITORY  https://github.com/ricab/scope_guard.git
  GIT_TAG         v1.1.0
)
FetchContent_MakeAvailable(scope_guard)
add_library(scope_guard INTERFACE)
target_include_directories(scope_guard INTERFACE ${scope_guard_SOURCE_DIR})

find_package(Boost 1.70 COMPONENTS context REQUIRED)

add_library(eff-unwind
  "include/eff-unwind.hpp"
  "src/eff-unwind.cpp"
)
target_compile_features(eff-unwind PUBLIC cxx_std_20)
set_target_properties(eff-unwind PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(eff-unwind PUBLIC include)
target_link_libraries(eff-unwind PUBLIC scope_guard fmt::fmt)
if(EFF_UNWIND_TRACE)
  target_compile_definitions(eff-unwind PUBLIC EFF_UNWIND_TRACE)
endif()
if(EFF_UNWIND_ASAN)
  target_link_options(eff-unwind
    BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
  )
endif()
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"
  AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL "16"
  AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "19")
  target_compile_definitions(eff-unwind PUBLIC EFF_UNWIND_CONCEPT_CLANG_WORKAROUND)
endif()

function(add_test BIN_NAME SRC_FILE)
  add_executable(${BIN_NAME} ${SRC_FILE})
  target_compile_features(${BIN_NAME} PRIVATE cxx_std_20)
  set_target_properties(${BIN_NAME} PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(${BIN_NAME} PRIVATE eff-unwind)
  target_compile_options(${BIN_NAME} PRIVATE -Wall -Wextra)
endfunction()

add_test(test_multi "test/micro/multi.cpp")
add_test(test_large_return "test/micro/large_return.cpp")
add_test(test_small_return "test/micro/small_return.cpp")
add_test(test_two_raise "test/micro/two_raise.cpp")
add_test(test_exception "test/micro/exception.cpp")
add_test(countdown "test/countdown.cpp")
add_test(fibonacci_recursive "test/fibonacci_recursive.cpp")
add_test(product_early "test/product_early.cpp")
add_test(generator "test/generator.cpp")
add_test(handler_sieve "test/handler_sieve.cpp")
add_test(iterator "test/iterator.cpp")
add_test(nqueens "test/nqueens.cpp")
add_test(parsing_dollars "test/parsing_dollars.cpp")
add_test(resume_nontail "test/resume_nontail.cpp")
## add_test(tree_explore "test/tree_explore.cpp")
add_test(triples "test/triples.cpp")
## add_test(test_generator "test/micro/generator.cpp")

if(EFF_UNWIND_BUILD_CPP_EFFECTS)
  add_library(cpp_effects INTERFACE)
  target_include_directories(cpp_effects INTERFACE bench/include)
  target_link_libraries(cpp_effects INTERFACE Boost::context)

  function(add_test_eff BIN_NAME SRC_FILE)
    add_executable(${BIN_NAME} ${SRC_FILE})
    target_compile_features(${BIN_NAME} PRIVATE cxx_std_20)
    set_target_properties(${BIN_NAME} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(${BIN_NAME} PRIVATE cpp_effects)
  endfunction()

  add_test_eff(test_eff_counter "bench/counter.cpp")
  add_test_eff(test_eff_counter10 "bench/counter10.cpp")
  add_test_eff(test_eff_mstate "bench/mstate.cpp")
  add_test_eff(test_eff_exception "bench/exception.cpp")
  add_test_eff(test_eff_noexception "bench/noexception.cpp")
endif()
