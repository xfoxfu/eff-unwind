// Test case "iterator"
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/iterator/main.kk

#include <cstdint>
#include <iostream>
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Emit : eff::command<> {
  int value;
};

void emit(int value) {
  eff::invoke_command(Emit{{}, value});
}

void range(int l, int u) {
  for (; l <= u; l++) {
    emit(l);
  }
}

template <typename Return, typename Body>
class EmitHandler : public eff::handler<Return, Body, Emit> {
 public:
  EmitHandler(uint64_t& s) : s(s) {}

 private:
  uint64_t& s;
  Body handle_command(Emit e, eff::resumption<Return()> r) override {
    s += e.value;
    return std::move(r).tail_resume();
  }
  Return handle_return(Body a) override { return a; }
};

uint64_t run(int n) {
  uint64_t s = 0;
  return eff::handle<EmitHandler<uint64_t, uint64_t>>(
      [&]() {
        range(0, n);
        return s;
      },
      s);
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 40000000) {
    assert(value == 800000020000000);
  }
#endif
  return 0;
}
