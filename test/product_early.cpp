// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

#ifdef TEST_RAII
int x = 0;
struct RAII {
  ~RAII() { fmt::println("destructor"); }
};
#endif

struct Abort : public effect<int, unit_t> {};

int product(std::vector<int>::const_iterator xs_begin,
    std::vector<int>::const_iterator xs_end) {
#ifdef TEST_RAII
  RAII raii;
#endif
  int product = 1;
  for (auto it = xs_begin; it != xs_end; it++) {
    if (*it == 0) {
      raise<Abort>(0);
      abort();  // unreachable
      return 0;
    } else {
      product *= *it;
    }
  }
  return product;
}

int run_product(std::vector<int>& xs) {
  return do_handle<int, Abort>([&]() { return product(xs.begin(), xs.end()); },
      [](auto r, auto resume, auto yield) -> unit_t { yield(r); });
}

int run(int n) {
  std::vector<int> xs(1001);
  xs.reserve(1001);
  for (int i = 0; i <= 1000; i++) {
    xs[i] = 1000 - i;
  }

  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += run_product(xs);
  }
  return sum;
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 100000) {
    assert(value == 0);
  }
#endif
  return 0;
}
