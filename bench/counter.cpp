// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Benchmark: Compare native exceptions with exceptions via effect handlers

#include <chrono>
#include <functional>
#include <iostream>

#include "cpp-effects/clause-modifiers.h"
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Set : eff::command<> {
  int newState;
};

struct Get : eff::command<int> {};

void set(int value) {
  eff::invoke_command(Set{{}, value});
}

int get() {
  return eff::invoke_command(Get{});
}

class State : public eff::handler<int, int, Set, Get> {
 public:
  State(int init) : state(init) {}

 private:
  int state;
  int handle_command(Set p, eff::resumption<int()> r) override {
    state = p.newState;
    return std::move(r).tail_resume();
  }
  int handle_command(Get p, eff::resumption<int(int)> r) override {
    return std::move(r).tail_resume(state);
  }
  int handle_return(int a) override { return a; }
};

int counter(int c) {
  auto i = get();
  if (i == 0) {
    return c;
  }
  set(i - 1);
  return counter(c + 1);
}

void test() {
  eff::handle<State>([]() -> int { return counter(1000); }, 1000);
}

int main() {
  for (int i = 0; i < 1000; i++) {
    test();
  }
  return 0;
}
