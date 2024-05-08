/**
 * Test case "counter"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/countdown/main.kk
 * This test case uses "Get" and "Set" effects to maintain a shared state.
 * It counts down from given "n"=1000 to "0" for 1_0000 times.
 */
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Get : public effect<uint64_t, uint64_t> {};
struct Set : public effect<uint64_t, uint64_t> {};

with_effect<int, Get, Set> countdown() {
  effect_ctx<int, Get, Set> ctx;
  auto i = ctx.raise<Get>(0);
  while (true) {
    i = ctx.raise<Get>(0);
    if (i == 0) {
      return ctx.ret(i);
    } else {
      ctx.raise<Set>(i - 1);
    }
  }
  assert(false);
  return ctx.ret(0);
}

void run(uint64_t n) {
  auto s = n;
  auto handle_get = handle<Get>(
      [&](uint64_t in, auto ctx) -> uint64_t { RESUME_THEN_BREAK(s); });
  auto handle_set = handle<Set>([&](uint64_t in, auto ctx) -> uint64_t {
    s = in;
    RESUME_THEN_BREAK(s);
  });
  countdown();
}

int main() {
  for (int i = 0; i < 1'000'0; i++) {
    run(1000);
  }
  return 0;
}
