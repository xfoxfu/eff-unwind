// Test case "iterator"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/iterator/main.kk

#include <iostream>
#include "eff-unwind.hpp"

typedef int unit_t;

struct Emit : public effect<int, unit_t> {};

with_effect<unit_t, Emit> range(int l, int u) {
  effect_ctx<int, Emit> ctx;
  for (; l <= u; l++) {
    ctx.raise<Emit>(l);
  }
  return ctx.ret(0);
}

uint64_t run(int n) {
  uint64_t s = 0;
  auto g = handle<Emit>([&s](int e, auto ctx) -> unit_t {
    s += e;
    RESUME_THEN_BREAK(0);
  });
  range(0, n);
  return s;
}

int main(int, char** argv) {
  std::cout << run(std::stoi(argv[1])) << std::endl;

  return 0;
}
