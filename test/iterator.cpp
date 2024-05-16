// Test case "iterator"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/iterator/main.kk

#include "eff-unwind.hpp"

struct Emit : public effect<int, /* actually void */ int> {};

with_effect</* actually void */ int, Emit> range(int l, int u) {
  effect_ctx<int, Emit> ctx;

  if (l > u) {
    return ctx.ret(0);
  }
  ctx.raise<Emit>(l);
  return range(l + 1, u);
}

int run(int n) {
  int s = 0;
  auto g = handle<Emit>([&s](int e, auto ctx) -> /* actually void */ int {
    s += e;
    RESUME_THEN_BREAK(0);
  });
  range(0, n);
  return s;
}

int main() {
  for (int i = 0; i < 1'0000; i++) {
    run(1000);
  }
  return 0;
}
