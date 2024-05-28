// Test case "handler_sieve"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/handler_sieve/main.kk

// FIXME:
// AddressSanitizer:DEADLYSIGNAL
// =================================================================
// ==44481==ERROR: AddressSanitizer: stack-overflow on address 0x00016f11bfe0
// (pc 0x0001004ec800 bp 0x00016f11cc60 sp 0x00016f11bea0 T0)
// #0 0x1004ec800 in thread-local wrapper routine for frames+0x4
//     (handler_sieve:arm64+0x100004800)
// #1 0x1004ec208 in handler_frame<Prime,
//     primes(int, int, int)::$_0>::invoke(unsigned long long,
//     resume_context<Prime>)+0x2c (handler_sieve:arm64+0x100004208)
// #2 0x1004ebd78 in Prime::resume_t effect_ctx<int,
//     Prime>::raise<Prime>(Prime::raise_t)+0xe8
//     (handler_sieve:arm64+0x100003d78)
// #3 0x1004ec208 in handler_frame<Prime,
//     primes(int, int, int)::$_0>::invoke(unsigned long long,
//     resume_context<Prime>)+0x2c (handler_sieve:arm64+0x100004208)
// SUMMARY: AddressSanitizer: stack-overflow (handler_sieve:arm64+0x100004800)
// in thread-local wrapper routine for frames+0x4
// ==44481==ABORTING

#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Prime : public effect<uint64_t, bool> {};

with_effect<int, Prime> primes(int i, int n, int a) {
  effect_ctx<int, Prime> ctx;
  if (i >= n) {
    return ctx.ret(a);
  }
  if (ctx.raise<Prime>(i)) {
    auto h = handle<Prime>([&i](int e, auto ctx) -> bool {
      if (e % i == 0) {
        RESUME_THEN_BREAK(false);
      } else {
        return effect_ctx<int, Prime>().raise<Prime>(e);
      }
    });
    return primes(i + 1, n, a + i);
  } else {
    return primes(i, n, a);
  }
}

int run(int n) {
  auto h =
      handle<Prime>([](int e, auto ctx) -> bool { RESUME_THEN_BREAK(true); });
  return primes(2, n, 0).value;
}

int main() {
  for (int i = 0; i < 1'0000; i++) {
    run(100);
  }
  return 0;
}
