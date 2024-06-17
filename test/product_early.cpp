// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef int unit_t;
typedef int char_t;

struct Abort : public effect<int, unit_t> {};

with_effect<unit_t, Abort> product(std::vector<int>::const_iterator xs_begin,
                                   std::vector<int>::const_iterator xs_end) {
  effect_ctx<unit_t, Abort> ctx;
  int product = 1;
  for (auto it = xs_begin; it != xs_end; it++) {
    if (*it == 0) {
      ctx.raise<Abort>(0);
      abort();  // unreachable
      return ctx.ret(0);
    } else {
      product *= *it;
    }
  }
  return ctx.ret(product);
}

int run_product(std::vector<int>& xs) {
  auto g = handle<Abort>([](auto r, auto ctx) -> unit_t { BREAK(r); });
  return product(xs.begin(), xs.end()).value;
}

unit_t run(int n) {
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
  std::cout << run(std::stoi(argv[1])) << std::endl;
  return 0;
}
