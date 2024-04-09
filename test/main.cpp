#include "eff-unwind.hpp"
#include "fmt/core.h"

struct RAII {
  ~RAII() {}
};

struct Exception : public effect<uint64_t, uint64_t> {};

with_effect<int, Exception> foobar() {
  effect_ctx<int, Exception> ctx;
  auto num = ctx.raise<Exception>(0);
  return ctx.ret(56);
}

uint64_t has_handler() {
#ifdef NDEBUG
  uint64_t sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  fmt::println("has_handler sp={:#x}", sp);
  print_frames();
#endif

  auto guard = handle<Exception>([](uint64_t in, auto ctx) -> uint64_t {
    print_frames();
    if (ctx.resume(42)) {
      return 0;
    }
    print_frames();
    fmt::println("non-tail resumption");
    return 0;
  });
  int num = 42;
  // RAII raii;
  int ret = foobar().value;
  print_frames();
  resume_nontail();
  return 24;
}

void test() {
  // FIXME: if Release mode is specified, cannot be handled correctly
  // need to tell the C++ optimizer that return value is not a constant
  auto val = has_handler();
}

int main() {
  int MAX = 1;  // '000'000;
  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < MAX; i++) {
    test();
  }
  auto end = std::chrono::high_resolution_clock::now();
  fmt::println(
      "{}ns    ({}ns per iteration)",
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(),
      (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
                .count() /
            MAX));
  return 0;
}
