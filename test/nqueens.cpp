// Test case "nqueens"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/nqueens/main.kk

#include <algorithm>
#include <iostream>
#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Pick : public effect<int, int> {};
struct Fail : public effect</* void */ int, /* void */ int> {};

bool safe(int queen, int diag, std::vector<int>& xs, size_t xs_i) {
  if (xs_i >= xs.size()) {
    return true;
  }

  auto& q = xs[xs_i];
  if (queen != q && queen != q + diag && queen != q - diag) {
    return safe(queen, diag + 1, xs, xs_i + 1);
  }
  return false;
}

with_effect<std::vector<int>, Pick, Fail> place(int size, int column) {
  effect_ctx<std::vector<int>, Pick, Fail> ctx;
  if (column == 0) {
    return ctx.ret(std::vector<int>());
  }

  auto rest = place(size, column - 1).value;
  auto next = ctx.raise<Pick>(size);
  if (safe(next, 1, rest, 0)) {
    rest.insert(rest.begin(), next);
    return ctx.ret(rest);
  } else {
    ctx.raise<Fail>(0);
    abort();
  }
}

int run(int n) {
  auto g1 = handle<Fail>([](int _, auto ctx) -> int { BREAK(0); });
  auto g2 = handle<Pick>([](int size, auto ctx) -> int {
    int a = 0;
    for (int i = 1; i <= size; i++) {
      fmt::println("resume={}", i);
      RESUME(i);
      fmt::println("resume'={}", i);
      a += 1;
    }
    BREAK(a);
  });
  place(n, n);
  // resume_nontail();
  return 1;
}

int main(int, char** argv) {
  std::cout << run(std::stoi(argv[1])) << std::endl;
  return 0;
}
