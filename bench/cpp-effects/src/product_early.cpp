// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include <vector>
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

#ifdef TEST_RAII
int x = 0;
struct RAII {
  ~RAII() { std::cout << "destructor" << std::endl; }
};
#endif

struct Abort : eff::command<> {
  int value;
};

int product(std::vector<int>::const_iterator xs_begin,
    std::vector<int>::const_iterator xs_end) {
#ifdef TEST_RAII
  RAII raii;
#endif
  int product = 1;
  for (auto it = xs_begin; it != xs_end; it++) {
    if (*it == 0) {
      eff::invoke_command(Abort{{}, 0});
      abort();  // unreachable
      return 0;
    } else {
      product *= *it;
    }
  }
  return product;
}

struct AbortHandler : eff::handler<int, int, Abort> {
  int handle_command(Abort p, eff::resumption<int()>) override {
    return p.value;
  }
  int handle_return(int v) override { return v; }
};

int run_product(std::vector<int>& xs) {
  return eff::handle<AbortHandler>(
      [&]() { return product(xs.begin(), xs.end()); });
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
