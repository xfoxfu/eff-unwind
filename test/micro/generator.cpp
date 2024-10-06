#include "eff-unwind.hpp"
#include "fmt/core.h"

struct yield : public effect<uint64_t, unit_t> {};

struct generator {
  uint64_t value;
  std::optional<saved_resumption<yield>> resumption;

  explicit generator(uint64_t value, saved_resumption<yield> resumption)
      : value(value), resumption(resumption) {}
  generator() : resumption({}) {}

  generator next() {
    if (resumption.has_value()) {
      auto res = resumption.value();
      resumption.reset();
      std::move(res).resume({});
      abort();  // unreachable
    } else {
      return {};
    }
  }
};

void iterate(int n) {
  for (int i = 0; i < n; i++) {
    raise<yield>(0);
  }
}

generator generate() {
  return do_handle<generator, yield>(
      []() {
        iterate(5);
        return generator();
      },
      [](uint64_t value, auto resume, auto yield) -> unit_t {
        yield(generator(value, resume.save_resumption()));
      });
}

int main() {
  auto gen = generate();
  while (gen.resumption.has_value()) {
    fmt::println("value = {}", gen.value);
    gen = gen.next();
  }
  return 0;
}
