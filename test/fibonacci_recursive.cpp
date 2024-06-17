/**
 * Test case "fibonacci_recursive"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/fibonacci_recursive/main.kk
 * This test case computes fibonacci fib(n=40) for 1_0000 times.
 */

#include <cstdint>
#include <iostream>

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
  std::cout << fib(std::stoi(argv[1])) << std::endl;
  return 0;
}
