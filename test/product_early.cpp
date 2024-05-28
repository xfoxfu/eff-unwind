// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef int unit_t;
typedef int char_t;

struct Abort : public effect<int, unit_t> {};

with_effect<unit_t, Abort> product(std::vector<int>::const_iterator xs_begin,
                                   std::vector<int>::const_iterator xs_end) {
  effect_ctx<unit_t, Abort> ctx;
  if (xs_begin == xs_end) {
    return ctx.ret(0);
  } else {
    if (*xs_begin == 0) {
      ctx.raise<Abort>(0);
      abort();
      return ctx.ret(0);
    } else {
      return ctx.ret((*xs_begin) * product(xs_begin + 1, xs_end).value);
    }
  }
}

unit_t run(std::vector<int> xs) {
  auto g = handle<Abort>([](auto r, auto ctx) -> unit_t { BREAK(r); });
  return product(xs.begin(), xs.end()).value;
}

int main() {
  int n = 1'000;

  std::vector<int> xs(1000);
  xs.reserve(1000);
  for (int i = 0; i < 1000; i++) {
    xs[i] = i;
  }

  for (int i = 0; i < n; i++) {
    run(xs);
  }
  return 0;
}
