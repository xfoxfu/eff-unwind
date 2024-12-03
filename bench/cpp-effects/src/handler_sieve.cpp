#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Prime : eff::command<bool> {
  int e;
};

bool prime(int e) {
  return eff::invoke_command(Prime{{}, e});
}

template <typename Return>
class PrimeHandler : public eff::handler<Return, int, Prime> {
 public:
  PrimeHandler(int i) : i(i) {}

 private:
  int i;
  int handle_command(Prime p, eff::resumption<Return(bool)> r) override {
    if (p.e % i == 0) {
      return std::move(r).tail_resume(false);
    } else {
      auto val = prime(p.e);
      return std::move(r).tail_resume(val);
    }
  }
  Return handle_return(Return a) override { return a; }
};

template <typename Return>
class PrimeHandlerBottom : public eff::handler<Return, int, Prime> {
 public:
  PrimeHandlerBottom() {}

 private:
  int handle_command(Prime, eff::resumption<Return(bool)> r) override {
    return std::move(r).tail_resume(true);
  }
  Return handle_return(Return a) override { return a; }
};

int primes(int i, int n, int a) {
  if (i >= n) {
    return a;
  }
  if (prime(i)) {
    return eff::handle<PrimeHandler<int>>(
        [&]() { return primes(i + 1, n, a + i); }, i);
  } else {
    return primes(i + 1, n, a);
  }
}

int run(int n) {
  return eff::handle<PrimeHandlerBottom<int>>(
      [&]() { return primes(2, n, 0); });
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = run(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 100) {
    assert(value == 1060);
  }
#endif
  return 0;
}
