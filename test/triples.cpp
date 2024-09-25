// triples
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/triples/main.kk

// module main
#include <iostream>
#include <tuple>
#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef int unit_t;

// import std/os/env

// effect flip
//   ctl flip() : bool
class flip : public effect<unit_t, bool> {};
// effect fail
//   ctl fail<a>() : a
template <typename a>
class fail : public effect<unit_t, a> {};

// fun choice(n : int)
//   if (n < 1)
//     then fail()
//   elif (flip())
//     then n
//   else choice(n - 1)
with_effect<int, flip, fail<int>> choice(int n) {
  effect_ctx<int, flip, fail<int>> ctx;
  while (n >= 1) {
    if (ctx.raise<flip>(0)) {
      return ctx.ret(n);
    } else {
      n = n - 1;
    }
  }
  ctx.raise<fail<int>>(0);
  return ctx.ret(0);  // unreachable
}

// fun triple(n : int, s: int)
//   val i = choice(n)
//   val j = choice(i - 1)
//   val k = choice(j - 1)
//   if i + j + k == s
//     then (i, j, k)
//   else fail()
std::tuple<int, int, int> triple(int n, int s) {
  fmt::println("choice on i");
  auto i = choice(n).value;
  fmt::println("choice on j");
  auto j = choice(i - 1).value;
  fmt::println("choice on k");
  auto k = choice(j - 1).value;
  if (i + j + k == s) {
    return {i, j, k};
  } else {
    effect_ctx<int, flip, fail<int>> ctx;
    ctx.raise<fail<int>>(0);
    return {0, 0, 0};  // unreachable
  }
}

// fun hash((a,b,c)) : int
//   (53 * a + 2809 * b + 148877 * c) % 1000000007
int hash(std::tuple<int, int, int> t) {
  auto [a, b, c] = t;
  return (53 * a + 2809 * b + 148877 * c) % 1000000007;
}

// fun run(n : int, s : int) : div int
//   with ctl flip()
//     ((resume(True) : int) + resume(False)) % 1000000007
//   with ctl fail()
//     0
//   hash(triple(n, s))
int run(int n, int s) {
  int i = 0;
  auto g1 = handle<flip>([&i](bool x, auto ctx) -> int {
    i += 1;
    fmt::println("resume=true [{}]", i);
    RESUME(true);
    fmt::println("resume=false [{}]", i);
    RESUME_THEN_BREAK(false);
  });
  auto g2 = handle<fail<int>>([](int x, auto ctx) -> int { BREAK(0); });
  auto r = triple(n, s);
  return hash(r);
}

// pub fun main()
//   val n = get-args().head("").parse-int-default(10)
//   val r = run(n, n)
//   println(r)
int main(int, char** argv) {
  auto n = std::stoi(argv[1]);
  std::cout << run(n, n) << std::endl;
  return 0;
}
