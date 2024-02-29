#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Exception : public effect<uint64_t, uint64_t> {};

with_effect<int, Exception> foobar() {
  effect_ctx<int, Exception> ctx;
  // auto num = ctx.raise<Exception>(0);
  return ctx.ret(56);
}

uint64_t has_handler() {
  auto guard = handle<Exception>(
      [](uint64_t in) -> std::unique_ptr<handler_result<uint64_t>> {
        // return handler_resume<uint64_t>(42);
        return handler_return<uint64_t>(72);
      });
  return foobar().value;
}

#include <iostream>
void test() {
  auto val = has_handler();
  assert(val == 72);
}

int main() {
  int MAX = 1'000'000;
  for (int i = 0; i < MAX; i++) {
    test();
  }
  return 0;
}
