// Test case "handler_sieve"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/handler_sieve/main.kk

#include <iostream>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Prime : public effect<uint64_t, bool> {};

int primes(int i, int n, int a) {
  if (i >= n) {
    return a;
  }
  if (raise<Prime>(i)) {
    return do_handle<int, Prime>([&]() { return primes(i + 1, n, a + i); },
        [i](int e) -> bool {
          if (e % i == 0) {
            return false;
          } else {
            auto val = raise<Prime>(e);
            return val;
          }
        });
  } else {
    return primes(i + 1, n, a);
  }
}

int run(int n) {
  return do_handle<int, Prime>(
      [&]() { return primes(2, n, 0); }, [](int) -> bool { return true; });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 100) {
    assert(value == 1060);
  }
#endif
  return 0;
}
