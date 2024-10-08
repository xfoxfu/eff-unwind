// Test case "iterator"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/iterator/main.kk

#include <cstdint>
#include <iostream>
#include "eff-unwind.hpp"

struct Emit : public effect<int, unit_t> {};

void range(int l, int u) {
  for (; l <= u; l++) {
    raise<Emit>(l);
  }
}

uint64_t run(int n) {
  uint64_t s = 0;
  return do_handle<uint64_t, Emit>(
      [&]() {
        range(0, n);
        return s;
      },
      [&s](int e) -> unit_t {
        s += e;
        return {};
      });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 40000000) {
    assert(value == 800000020000000);
  }
#endif
  return 0;
}
