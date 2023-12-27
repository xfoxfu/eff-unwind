#include "eff-unwind.hpp"

struct RAII {
  ~RAII() { fmt::println("RAII destructor"); }
};

struct Exception : public effect<uint64_t, uint64_t> {};

with_effect<Exception, int> foobar() {
  effect_ctx<Exception, int> ctx;
  fmt::println("before raise");
  auto num = ctx.raise(0);
  fmt::println("after raise: {}", std::any_cast<uint64_t>(num));
  fmt::println("foobar continue");
  return ctx.ret(56);
}

uint64_t has_handler() {
  auto guard = handle<Exception>(
      [](uint64_t in) -> std::unique_ptr<handler_result<uint64_t>> {
        // return handler_resume<uint64_t>(42);
        return handler_return<uint64_t>(72);
      });
  int num = 42;
  RAII raii;
  fmt::println("has_handler begin");
  int ret = foobar().value;
  fmt::println("has_handler end: {} ret={}", num, ret);
  return 24;
}

int main() {
  auto val = has_handler();
  // FIXME: if Release mode is specified, cannot be handled correctly
  // need to tell the C++ optimizer that return value is not a constant
  fmt::println("main end: {}", val);
  return 0;
}
