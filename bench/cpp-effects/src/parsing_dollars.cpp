// parsing_dollars
// https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/parsing_dollars/main.kk

#include <cstdint>
#include <iostream>
#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

typedef int char_t;

struct Read : eff::command<char_t> {};
struct Emit : eff::command<> {
  int value;
};
struct Stop : eff::command<> {};

char_t newline() {
  return 10;
}
bool is_newline(char_t c) {
  return c == 10;
}

char_t dollar() {
  return 36;
}
bool is_dollar(char_t c) {
  return c == 36;
}

void parse() {
  int a = 0;
  char_t c;
  while ((c = eff::invoke_command(Read{
              {},
          }))) {
    if (is_dollar(c)) {
      a += 1;
    } else if (is_newline(c)) {
      eff::invoke_command(Emit{{}, a});
      a = 0;
    } else {
      eff::invoke_command(Stop{
          {},
      });
    }
  }
  assert(false);  // unreachable
}

template <typename Return, typename Body>
class ReadHandler : public eff::handler<Return, Body, Read> {
 public:
  ReadHandler(int& i, int& j, int& n) : i(i), j(j), n(n) {}

 private:
  int& i;
  int& j;
  int& n;
  Body handle_command(Read, eff::resumption<Return(char_t)> r) override {
    if (i > n) {
      eff::invoke_command(Stop{{}});
    } else if (j == 0) {
      i += 1;
      j = i;
      return std::move(r).tail_resume(newline());
    } else {
      j -= 1;
      return std::move(r).tail_resume(dollar());
    }
  }
  Return handle_return() override { return; }
};

template <typename Return, typename Body>
class EmitHandler : public eff::handler<Return, Body, Emit> {
 public:
  EmitHandler(int32_t& s) : s(s) {}

 private:
  int32_t& s;
  Body handle_command(Emit e, eff::resumption<Return()> r) override {
    s += e.value;
    return std::move(r).tail_resume();
  }
  Return handle_return(Body a) override { return a; }
};

template <typename Return, typename Body>
class StopHandler : public eff::handler<Return, Body, Stop> {
 public:
  StopHandler() {}

 private:
  Body handle_command(Stop, eff::resumption<Return()>) override { return; }
  Return handle_return() override { return; }
};

void feed(int n) {
  int i = 0, j = 0;
  eff::handle<ReadHandler<void, void>>([]() { parse(); }, i, j, n);
}

void catchh(int n) {
  eff::handle<StopHandler<void, void>>([n]() { feed(n); });
}

uint32_t sum(int n) {
  int s = 0;
  return eff::handle<EmitHandler<uint32_t, uint32_t>>(
      [&]() {
        catchh(n);
        return s;
      },
      s);
}

int main(int, char** argv) {
  auto size = std::stoi(argv[1]);
  auto value = sum(size);
  std::cout << value << std::endl;
#ifndef NDEBUG
  if (size == 20000) {
    assert(value == 200010000);
  }
#endif
  return 0;
}
