#include <fmt/core.h>
#include <cstdint>
#include "eff-unwind.hpp"

struct rayon {
  uint32_t x;
};

struct Yield : public effect<unit_t, unit_t> {};

int main() {
  volatile auto r = do_handle<rayon, Yield>(
      []() -> rayon {
        raise<Yield>({});
        return {2};
      },
      [](unit_t, auto resume, auto yield) -> unit_t {
        rayon r = {1};
        yield(r);
      });
  fmt::println("{}", r.x);
}
