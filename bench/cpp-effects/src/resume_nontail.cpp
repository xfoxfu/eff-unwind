#include <iostream>
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Operator : eff::command<> {
  int x;
};

int loop(int n, int s) {
  for (int i = n; i > 0; i--) {
    eff::invoke_command(Operator{{}, i});
  }

  return s;
}

struct OperatorHandler : eff::handler<int, int, Operator> {
  int handle_command(Operator p, eff::resumption<int()> r) override {
    auto y = std::move(r).resume();
    return abs(p.x - (503 * y) + 37) % 1009;
  }
  int handle_return(int v) override { return v; }
};

int run(int n, int s) {
  return eff::handle<OperatorHandler>([=]() { return loop(n, s); });
}

int repeat(int n) {
  int ret = 0;
  for (int i = 0; i < 1000; i++) {
    ret = run(n, 0);
  }
  return ret;
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = repeat(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 200) {
    assert(value == 632);
  }
#endif
  return 0;
}
