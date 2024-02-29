#include <_types/_uint64_t.h>
#include "eff-unwind.hpp"

// import std/num/int32

// effect st {
//   control get() : int32
//   control set(i : int32)  : ()
// }

struct Get : public effect<uint64_t, uint64_t> {};
struct Set : public effect<uint64_t, uint64_t> {};

// fun state( init : int32, action : () -> <st|e> a ) : e a {
//   with return(f){ f(init) }
//   with handler {
//     return(x){ fn(s){ x } }
//     rcontrol get(){ fn(s){ rcontext.resume(s)(s) } }
//     rcontrol set(s){ fn(_){ rcontext.resume(())(s) } }
//   }
//   action()
// }

// fun counter( c : int32 ) : _ int {
//   val i = get()
//   if (i==zero) then c.int else {
//     set(i.dec)
//     counter(c.inc)
//   }
// }
with_effect<uint64_t, Get, Set> counter(uint64_t c) {
  effect_ctx<uint64_t, Get, Set> ctx;
  auto i = ctx.raise<Get>(0);
  if (i == 0) {
    return ctx.ret(c);
  }
  ctx.raise<Set>(i - 1);
  return counter(c - 1);
}

// fun main() {
//   with state(10100100.int32)
//   counter(zero).println
// }
int main() {
  auto val = 50000;
  auto guard1 =
      handle<Get>([&](uint64_t) -> std::unique_ptr<handler_result<uint64_t>> {
        return handler_resume<uint64_t>(val);
      });
  auto guard2 = handle<Set>(
      [&](uint64_t newVal) -> std::unique_ptr<handler_result<uint64_t>> {
        return handler_resume<uint64_t>(val = newVal);
      });
  counter(0);
  return 0;
}
