#include "eff-unwind.hpp"
#include "fmt/base.h"
#include "fmt/core.h"

struct RAII {
  ~RAII() { fmt::println("RAII"); }
};

struct Exception : public effect<uint64_t, uint64_t> {};

int foobar() {
  auto num = raise<Exception>(0);
  fmt::println("Exception resume={}", num);
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
  // RAII raii;
#ifdef EFF_UNWIND_TRACE
        print_frames("handler pre");
#endif
        resume(42);
#ifdef EFF_UNWIND_TRACE
        fmt::println("handler post");
        print_frames("handler post");
#endif
        fmt::println("non-tail resumption");
        resume(43);
        fmt::println("non-tail resumption2");
        yield(0);
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
