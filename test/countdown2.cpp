/**
 * Test case "counter"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/countdown/main.kk
 * This test case uses "Get" and "Set" effects to maintain a shared state.
 * It counts down from given "n"=1000 to "0" for 1_0000 times.
 */
#include <iostream>
#include "eff-unwind.hpp"

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

int run(uint64_t n) {
  auto s = n;

  return do_handle<int, Get>(
      [&]() -> int {
        return do_handle<int, Set>([]() -> int { return countdown().value; },
            [&](uint64_t in, auto ctx, auto b) -> uint64_t {
              s = in;
              RESUME_THEN_BREAK(s);
            });
      },
      [&](uint64_t in, auto ctx, auto b) -> uint64_t { RESUME_THEN_BREAK(s); });
}

int main(int, char** argv) {
  std::cout << run(std::stoi(argv[1])) << std::endl;
  return 0;
}
