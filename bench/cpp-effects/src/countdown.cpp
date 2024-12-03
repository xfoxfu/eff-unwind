// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Benchmark: Compare native exceptions with exceptions via effect handlers

#include <functional>
#include <iostream>

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
  int handle_command(Get, eff::resumption<int(int)> r) override {
    return std::move(r).tail_resume(state);
  }
  int handle_return(int a) override { return a; }
};

int countdown() {
  auto i = get();
  while (true) {
    i = get();
    if (i == 0)
      return i;
    set(i - 1);
  }
  return 0;
}

int run(uint64_t n) {
  return eff::handle<State>(countdown, n);
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 200000000) {
    assert(value == 0);
  }
#endif
  return 0;
}
