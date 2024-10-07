#include <cstdint>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct RAII {
  ~RAII() {}
};

struct Exception : public effect<uint64_t, uint64_t> {};

int foobar() {
  auto num1 = raise<Exception>(0);
  auto num2 = raise<Exception>(0);
  return 56;
}

uint64_t has_handler() {
  return do_handle<uint64_t, Exception>(
      []() {
        int num = 42;
        int ret = foobar();
        return 0;
      },
      [](uint64_t in, auto resume, auto yield) -> uint64_t {
        resume(42);
        fmt::println("non-tail resumption");
        return 0;
      });
}

void test() {
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
  fmt::println(
      "{:8}ns ({}ns per iteration)", duration_ns, (int)(duration_ns / MAX));
  return 0;
}
