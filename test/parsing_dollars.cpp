// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef int unit_t;
typedef int char_t;

struct Read : public effect<unit_t, char_t> {};
struct Emit : public effect<int, unit_t> {};
struct Stop : public effect<unit_t, unit_t> {};

char_t newline() {
  return 10;
}
bool is_newline(char_t c) {
  return c == 10;
}

char_t dollar() {
  return 36;
}
bool is_dollar(char_t c) {
  return c == 36;
}

with_effect<unit_t, Read, Emit, Stop> parse(int a) {
  effect_ctx<unit_t, Read, Emit, Stop> ctx;
  auto c = ctx.raise<Read>(0);
  if (is_dollar(c)) {
    parse(a + 1);
  } else if (is_newline(c)) {
    ctx.raise<Emit>(a);
    parse(0);
  } else {
    ctx.raise<Stop>(0);
  }
  fmt::println("unreachable! = parse");
  abort();
}

unit_t run(int n) {
  int s = 0;
  auto g1 = handle<Emit>([&s](int e, auto ctx) -> unit_t {
    s += e;
    RESUME(0);
  });
  auto g2 = handle<Stop>([](auto _, auto ctx) -> unit_t { BREAK(0); });
  int i = 0, j = 0;
  auto g3 = handle<Read>([&i, &j, &n](auto _, auto ctx) -> unit_t {
    if (i > n) {
      effect_ctx<unit_t, Stop>().raise<Stop>(0);
    } else if (j == 0) {
      i += 1;
      j = i;
      RESUME(newline());
    } else {
      j -= 1;
      RESUME(dollar());
    }
  });
  resume_nontail();
  fmt::println("unreachable! = run");
  abort();
}

int main() {
  for (int i = 0; i < 10; i++) {
    run(10);
  }
  return 0;
}
