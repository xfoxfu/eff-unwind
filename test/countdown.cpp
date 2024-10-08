/**
 * Test case "counter"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/countdown/main.kk
 * This test case uses "Get" and "Set" effects to maintain a shared state.
 * It counts down from given "n"=1000 to "0" for 1_0000 times.
 */
#include <iostream>
#include "eff-unwind.hpp"

struct Get : public effect<unit_t, uint64_t> {};
struct Set : public effect<uint64_t, uint64_t> {};

int countdown() {
  auto i = raise<Get>({});
  while (true) {
    i = raise<Get>({});
    if (i == 0) {
      return i;
    } else {
      raise<Set>(i - 1);
    }
  }
  assert(false);
  return 0;
}

int run(uint64_t n) {
  auto s = n;

  return do_handle<int, Get>(
      [&]() -> int {
        return do_handle<int, Set>([]() -> int { return countdown(); },
            [&](uint64_t in) -> uint64_t {
              s = in;
              return s;
            });
      },
      [&](unit_t) -> uint64_t { return s; });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 200000000) {
    assert(value == 0);
  }
#endif
  return 0;
}
