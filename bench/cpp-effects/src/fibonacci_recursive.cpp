/**
 * Test case "fibonacci_recursive"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/fibonacci_recursive/main.kk
 * This test case computes fibonacci fib(n=40) for 1_0000 times.
 */

#include <iostream>
#ifndef NDEBUG
#include <cassert>
#endif

int fib(int n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = fib(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 42) {
    assert(value == 267914296);
  }
#endif
  return 0;
}
