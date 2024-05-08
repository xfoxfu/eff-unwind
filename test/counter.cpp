/**
 * Test case "counter"
 * @see https://github.com/daanx/effect-bench/blob/main/src/counter.kk
 */
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Get : public effect<uint64_t, uint64_t> {};
struct Set : public effect<uint64_t, uint64_t> {};

with_effect<int, Get, Set> countdown() {
  effect_ctx<int, Get, Set> ctx;
  auto i = ctx.raise<Get>(0);
  while (i < 2000) {
    i = ctx.raise<Get>(0);
    if (i == 0) {
      return ctx.ret(i);
    } else {
      ctx.raise<Set>(i - 1);
    }
  }
  assert(false);
}

void run(uint64_t n) {
  auto s = n;
  auto handle_get = handle<Get>([&](uint64_t in, auto ctx) -> uint64_t {
    ctx.break_resume(s);
    return 0;
  });
  auto handle_set = handle<Set>([&](uint64_t in, auto ctx) -> uint64_t {
    s = in;
    ctx.break_resume(s);
    return 0;
  });
  countdown();
}

int main() {
  for (int i = 0; i < 1000; i++) {
    run(1000);
  }
  return 0;
}
