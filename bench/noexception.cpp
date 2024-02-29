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

struct Error : eff::command<> {};

class Catch : public eff::handler<void, void, Error> {
  void handle_return() final override {}
  void handle_command(Error, eff::resumption<void()>) final override {}
};

void testHandlers() {
  eff::handle<Catch>([]() { /* eff::invoke_command(Error{});  */ });
}

int main() {
  for (int i = 0; i < 1'000'000; i++) {
    testHandlers();
  }
  return 0;
}
