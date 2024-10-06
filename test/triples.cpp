// triples
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/triples/main.kk

#include <iostream>
#include <tuple>
#include "eff-unwind.hpp"

class flip : public effect<unit_t, bool> {};
template <typename a>
class fail : public effect<unit_t, a> {};

int choice(int n) {
  while (n >= 1) {
    if (raise<flip>({})) {
      return n;
    } else {
      n = n - 1;
    }
  }
  raise<fail<int>>({});
  return 0;  // unreachable
}

std::tuple<int, int, int> triple(int n, int s) {
  auto i = choice(n);
  auto j = choice(i - 1);
  auto k = choice(j - 1);
  if (i + j + k == s) {
    return {i, j, k};
  } else {
    raise<fail<int>>({});
    return {0, 0, 0};  // unreachable
  }
}

int hash(std::tuple<int, int, int> t) {
  auto [a, b, c] = t;
  return (53 * a + 2809 * b + 148877 * c) % 1000000007;
}

int run(int n, int s) {
  return do_handle<int, flip>(
      [&]() -> int {
        return do_handle<int, fail<int>>(
            [&]() {
              auto r = triple(n, s);
              return hash(r);
            },
            [](int x, auto resume, auto yield) -> int { yield(0); });
      },
      [](unit_t, auto resume, auto yield) -> bool {
        auto v1 = resume(true);
        auto v2 = resume(false);
        yield((v1 + v2) % 1000000007);
      });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size, size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 300) {
    assert(value == 460212934);
  }
#endif
  return 0;
}
