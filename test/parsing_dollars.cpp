// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
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

with_effect<unit_t, Read, Emit, Stop> parse() {
  effect_ctx<unit_t, Read, Emit, Stop> ctx;
  int a = 0;
  char_t c;
  while ((c = ctx.raise<Read>(0))) {
    if (is_dollar(c)) {
      a += 1;
    } else if (is_newline(c)) {
      ctx.raise<Emit>(a);
      a = 0;
    } else {
      ctx.raise<Stop>(0);
    }
  }
  return ctx.ret(0);  // unreachable
}

void feed(int n) {
  int i = 0, j = 0;
  auto g = handle<Read>([&i, &j, &n](auto _, auto ctx) -> unit_t {
    if (i > n) {
      effect_ctx<unit_t, Stop>().raise<Stop>(0);
    } else if (j == 0) {
      i += 1;
      j = i;
      RESUME_THEN_BREAK(newline());
    } else {
      j -= 1;
      RESUME_THEN_BREAK(dollar());
    }
    return {};
  });
  parse();
}

void catchh(int n) {
  auto g = handle<Stop>([](auto _, auto ctx) -> unit_t { BREAK(0); });
  feed(n);
}

uint32_t sum(int n) {
  int s = 0;
  auto g = handle<Emit>([&s](int e, auto ctx) -> unit_t {
    s += e;
    RESUME_THEN_BREAK(0);
  });
  catchh(n);
  fmt::println("after catchh");
  return s;
}

int main(int, char** argv) {
  std::cout << sum(std::stoi(argv[1])) << std::endl;
  return 0;
}
