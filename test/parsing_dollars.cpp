// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <iostream>
#include "eff-unwind.hpp"
#include "fmt/core.h"

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

void parse() {
  int a = 0;
  char_t c;
  while ((c = raise<Read>({}))) {
    if (is_dollar(c)) {
      a += 1;
    } else if (is_newline(c)) {
      raise<Emit>(a);
      a = 0;
    } else {
      raise<Stop>({});
    }
  }
  assert(false);  // unreachable
}

void feed(int n) {
  int i = 0, j = 0;
  do_handle<unit_t, Read>(
      []() {
        parse();
        return unit_t();
      },
      [&i, &j, &n](auto _, auto resume, auto yield) -> char_t {
        if (i > n) {
          raise<Stop>({});
        } else if (j == 0) {
          i += 1;
          j = i;
          return newline();
        } else {
          j -= 1;
          return dollar();
        }
        return {};
      });
}

void catchh(int n) {
  do_handle<unit_t, Stop>(
      [n]() -> unit_t {
        feed(n);
        return {};
      },
      [](auto _, auto resume, auto yield) -> unit_t { yield({}); });
}

uint32_t sum(int n) {
  int s = 0;
  return do_handle<uint32_t, Emit>(
      [&]() {
        catchh(n);
        return s;
      },
      [&s](int e, auto resume, auto yield) -> unit_t {
        s += e;
        return {};
      });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = sum(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 20000) {
    assert(value == 200010000);
  }
#endif
  return 0;
}
