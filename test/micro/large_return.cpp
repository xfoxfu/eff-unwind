#include <fmt/core.h>
#include <cstdint>
#include "eff-unwind.hpp"

struct rayon {
  uint32_t x, y, z, w;  //, z;
};

struct Yield : public effect<unit_t, unit_t> {};

int main() {
  auto r = do_handle<rayon, Yield>(
      []() -> rayon {
        raise<Yield>({});
        return {5, 6, 7, 8};
      },
      [](unit_t, auto resume, auto yield) -> unit_t {
        rayon r = {1, 2, 3, 4};
        yield(r);
      });
  fmt::println("{} {} {} {}", r.x, r.y, r.z, r.w);
}
