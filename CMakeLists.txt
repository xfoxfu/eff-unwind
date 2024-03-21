cmake_minimum_required(VERSION 3.14.0)
project(eff-unwind VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.3.2
)

# FetchContent_MakeAvailable(Catch2)

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

find_package(Boost 1.70 COMPONENTS context REQUIRED)

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
# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#   set(
#     CMAKE_C_FLAGS
#     "${CMAKE_C_FLAGS} -Werror -fsanitize=undefined -fsanitize=address"
#   )
#   target_link_options(tests
#     BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
#   )
# endif()

function(add_test BIN_NAME SRC_FILE)
  add_executable(${BIN_NAME} ${SRC_FILE})
  target_compile_features(${BIN_NAME} PRIVATE cxx_std_20)
  set_target_properties(${BIN_NAME} PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(${BIN_NAME} PRIVATE eff-unwind)
  # if(CMAKE_BUILD_TYPE MATCHES "Debug")
  # target_link_options(${BIN_NAME}
  #   BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
  # )
  # endif()
endfunction()

add_test(test_main "test/main.cpp")
add_test(test_counter "test/counter.cpp")
# add_test(test_counter10 "test/counter10.cpp")
# add_test(test_mstate "test/mstate.cpp")
# add_test(test_exception "test/exception.cpp")
# add_test(test_noexception "test/noexception.cpp")

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
