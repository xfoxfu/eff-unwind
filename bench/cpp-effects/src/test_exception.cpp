#include <cstdint>
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Exception : eff::command<> {};

int foobar() {
  eff::invoke_command(Exception{});
  return 56;
}

template <typename Return, typename Body>
class ExceptionHandler : public eff::handler<Return, Body, Exception> {
 public:
  Body handle_command(Exception, eff::resumption<Return()>) override {
    return 72;
  }
  Return handle_return(Body a) override { return a; }
};

uint64_t has_handler() {
  return eff::handle<ExceptionHandler<uint64_t, uint64_t>>(
      []() -> uint64_t { return foobar(); });
}

void test() {
  auto val = has_handler();
  (void)val;
  assert(val == 72);
}

int main() {
  int MAX = 1'000'000;
  for (int i = 0; i < MAX; i++) {
    test();
  }
  return 0;
}
