// import std/num/int32
#include <memory>
#include "eff-unwind.hpp"
#include "fmt/core.h"

// effect st {
//   control get() : int32
//   control set(i : int32)  : ()
// }
struct Get : public effect<int, int> {};
struct Set : public effect<int, int> {};

// fun state( i : int32, action : () -> <st|e> a ) : e a {
//   var s := i
//   with {
//     rcontrol get(){ rcontext.resume(s) }
//     rcontrol set(x){ s := x; rcontext.resume(()) }
//   }
//   action()
// }

// fun counter( c : int32 ) : <st,div> int32 {
//   val i = get()
//   if (i==zero) then c else {
//     set(i.dec)
//     counter(c.inc)
//   }
// }
with_effect<int, Get, Set> counter(int c) {
  effect_ctx<int, Get, Set> ctx;
  auto i = ctx.raise<Get>(0);
  if (i == 0) {
    return ctx.ret(c);
  } else {
    ctx.raise<Set>(i - 1);
    return counter(c + 1);
  }
}

// effect val ask : int
struct Ask : public effect<int, int> {};

// fun reader( init : int, action ) {
//   //println("enter reader " ++ init.show)
//   //with finally{ println("leave reader " ++ init.show) }
//   with val ask = init
//   action()
// }

// // at this point (koka v2.0.16) koka does not implement a tail-call for
// // a control resume; which means we use run out of stack on a 100M
// iteration...
// // instead we run the test 100_000 times over 1000 iterations. (That means we
// // may get a bit faster if proper tail calling would be implemented)
// fun test() {
//   with state(1000.int32)
//   with reader(1)
//   with reader(2)
//   with reader(3)
//   with reader(4)
//   with reader(5)
//   with reader(6)
//   with reader(7)
//   with reader(8)
//   with reader(9)
//   with reader(10)
//   counter(zero)
// }

// fun testN( n : int32, acc : int32 = zero) {
//   if (n.is-zero) then println(acc.int) else {
//     testN(n.dec, test() + acc)
//   }
// }

// fun main() {
//   testN(100100.int32)
//   //test()
// }

void test() {
  auto s = 1000;
  auto handle_get =
      handle<Get>([&](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume<int>(s);
      });
  auto handle_set =
      handle<Set>([&](int in) -> std::unique_ptr<handler_result<int>> {
        s = in;
        return handler_resume<int>(s);
      });
  auto handle_ask8 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask2 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask3 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask4 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask5 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask6 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask7 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask1 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask9 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  auto handle_ask10 =
      handle<Ask>([](int in) -> std::unique_ptr<handler_result<int>> {
        return handler_resume(0);
      });
  counter(0);
}

int main() {
  for (int i = 0; i < 1000; i++) {
    test();
  }
}
