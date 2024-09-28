// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Operator : public effect<int, unit_t> {};

int loop(int n, int s) {
  for (int i = 0; i < n; i++) {
    raise<Operator>(i);
  }

  return s;
}

int run(int n, int s) {
  return do_handle<int, Operator>([=]() { return loop(n, s); },
      [](int x, auto resume, auto yield) -> unit_t {
        int y = resume({});
        yield(abs(x - (503 * y) + 37) % 1009);
      });
}

int repeat(int n) {
  for (int i = 0; i < 1000; i++) {
    run(n, 0);
  }
  return 0;
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = repeat(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 200) {
    assert(value == 632);
  }
#endif
  return 0;
}
