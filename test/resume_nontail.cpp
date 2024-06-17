// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef int unit_t;
typedef int char_t;

struct Operator : public effect<int, unit_t> {};

with_effect<int, Operator> loop(int n, int s) {
  effect_ctx<int, Operator> ctx;

  for (int i = 0; i < n; i++) {
    ctx.raise<Operator>(i);
  }

  return ctx.ret(s);
}

int run(int n, int s) {
  auto g = handle<Operator>([](int x, auto ctx) -> int {
    RESUME(0);  // TODO: get resumption value
    int y = 0;
    return abs(x - (503 * y) + 37) % 1009;
  });
  return loop(n, s).value;
}

int repeat(int n) {
  for (int i = 0; i < 1000; i++) {
    run(n, 0);
  }
  return 0;
}

int main(int, char** argv) {
  std::cout << repeat(std::stoi(argv[1])) << std::endl;
  return 0;
}
