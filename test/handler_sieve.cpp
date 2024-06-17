// Test case "handler_sieve"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/handler_sieve/main.kk

#include <iostream>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Prime : public effect<uint64_t, bool> {};

with_effect<int, Prime> primes(int i, int n, int a) {
  effect_ctx<int, Prime> ctx;
  if (i >= n) {
    return ctx.ret(a);
  }
  if (ctx.raise<Prime>(i)) {
    // fmt::println("register handler for {}", i);
    auto h = handle<Prime>([i](int e, auto ctx) -> bool {
      // fmt::println("test {} / {} = {}", e, i, e % i);
      if (e % i == 0) {
        // fmt::println("break false");
        RESUME_THEN_BREAK(false);
      } else {
        // fmt::println("raise {}", e);
        auto val = ctx.template raise<Prime>(e);
        // fmt::println("get {}", val);
        RESUME_THEN_BREAK(ctx.template raise<Prime>(e));
      }
    });
    return primes(i + 1, n, a + i);
  } else {
    return primes(i + 1, n, a);
  }
}

int run(int n) {
  auto h =
      handle<Prime>([](int e, auto ctx) -> bool { RESUME_THEN_BREAK(true); });
  return primes(2, n, 0).value;
}

int main(int, char** argv) {
  std::cout << run(std::stoi(argv[1])) << std::endl;
  return 0;
}
