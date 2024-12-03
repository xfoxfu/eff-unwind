#include <cassert>
#include <cstdint>

struct Exception {};

int foobar() {
  throw Exception();
  return 56;
}

uint64_t has_handler() {
  try {
    return foobar();
  } catch (Exception& e) {
    return 72;
  }
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
