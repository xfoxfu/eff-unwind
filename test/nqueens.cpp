// Test case "nqueens"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/nqueens/main.kk

#include <iostream>
#include <vector>
#include "eff-unwind.hpp"
#include "fmt/core.h"

struct Pick : public effect<int, int> {};
struct Fail : public effect<unit_t, unit_t> {};

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

std::vector<int> place(int size, int column) {
  if (column == 0) {
    return std::vector<int>();
  }

  auto rest = place(size, column - 1);
  auto next = raise<Pick>(size);
  if (safe(next, 1, rest, 0)) {
    rest.insert(rest.begin(), next);
    return rest;
  } else {
    raise<Fail>({});
    abort();
  }
}

int run(int n) {
  return do_handle<int, Fail>(
      [&]() {
        return do_handle<int, Pick>(
            [&]() {
              place(n, n);
              return 1;
            },
            [](int size, auto resume, auto yield) -> int {
              int a = 0;
              for (int i = 1; i <= size; i++) {
                resume(i);
                a += 1;
              }
              yield(a);
            });
      },
      [](unit_t _, auto resume, auto yield) -> unit_t { yield(0); });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 12) {
    assert(value == 14200);
  }
#endif
  return 0;
}
