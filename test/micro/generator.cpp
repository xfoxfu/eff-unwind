#include "eff-unwind.hpp"
#include "fmt/core.h"

typedef uint64_t unit_t;

struct generator {
  uint64_t value;
  std::optional<saved_resumption> resumption;

  explicit generator(uint64_t value, saved_resumption resumption)
      : value(value), resumption(resumption) {}
  generator() : resumption({}) {}

  generator next() {
    if (resumption.has_value()) {
      auto res = resumption.value();
      resumption.reset();
      res.resume();
      abort();  // unreachable
    } else {
      return {};
    }
  }
};

struct yield : public effect<uint64_t, unit_t> {};

with_effect<unit_t, yield> iterate(int n) {
  effect_ctx<unit_t, yield> ctx;
  for (int i = 0; i < n; i++) {
    ctx.raise<yield>(0);
  }
  return ctx.ret(0);
}

generator generate() {
  auto g = handle<yield>([](uint64_t value, auto ctx) -> generator {
    auto resumption = ctx.save_resumption();
    if (!resumption.has_value()) {
      // resume
      return {};
    }
    // saved resumption
    BREAK(generator(value, resumption.value()));
  });
  iterate(5);
  return generator();
}

int main() {
  auto gen = generate();
  while (gen.resumption.has_value()) {
    fmt::println("value = {}", gen.value);
    gen = gen.next();
  }
  return 0;
}
