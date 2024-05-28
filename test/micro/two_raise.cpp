#include "eff-unwind.hpp"
#include "fmt/core.h"

struct RAII {
  ~RAII() {}
};

struct Exception : public effect<uint64_t, uint64_t> {};

with_effect<int, Exception> foobar() {
  effect_ctx<int, Exception> ctx;
  auto num1 = ctx.raise<Exception>(0);
  auto num2 = ctx.raise<Exception>(0);
  return ctx.ret(56);
}

uint64_t has_handler() {
  auto guard = handle<Exception>([](uint64_t in, auto ctx) -> uint64_t {
    if (ctx.resume(42)) {
      return 0;
    }
    fmt::println("non-tail resumption");
    return 0;
  });
  int num = 42;
  int ret = foobar().value;
  return 0;
}

void test() {
  // FIXME: if Release mode is specified, cannot be handled correctly
  // need to tell the C++ optimizer that return value is not a constant
  auto val = has_handler();
}

int main() {
  volatile int MAX = 1;
  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < MAX; i++) {
    test();
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
  fmt::println("{:8}ns ({}ns per iteration)", duration_ns,
               (int)(duration_ns / MAX));
  return 0;
}
