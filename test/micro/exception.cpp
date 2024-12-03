#include <cstdint>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Exception : public effect<unit_t, unit_t> {};

int foobar() {
  raise<Exception>({});
  return 56;
}

uint64_t has_handler() {
  return do_handle<uint64_t, Exception>([]() -> uint64_t { return foobar(); },
      [](unit_t in, auto resume, auto yield) -> unit_t { yield(72); });
}

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
