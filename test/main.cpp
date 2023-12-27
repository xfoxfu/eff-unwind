#include "eff-unwind.hpp"
#include "fmt/core.h"

struct RAII {
  ~RAII() {}
};

struct Exception : public effect<uint64_t, uint64_t> {};

with_effect<Exception, int> foobar() {
  effect_ctx<Exception, int> ctx;
  auto num = ctx.raise(0);
  return ctx.ret(56);
}

uint64_t has_handler() {
  auto guard = handle<Exception>(
      [](uint64_t in) -> std::unique_ptr<handler_result<uint64_t>> {
        // return handler_resume<uint64_t>(42);
        return handler_return<uint64_t>(72);
      });
  int num = 42;
  // RAII raii;
  int ret = foobar().value;
  return 24;
}

void test() {
  // FIXME: if Release mode is specified, cannot be handled correctly
  // need to tell the C++ optimizer that return value is not a constant
  auto val = has_handler();
}

int main() {
  int MAX = 1'000'000;
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
