#include <fmt/core.h>
#include <csetjmp>
#include <vector>

std::vector<char> SAVED_STACK;
jmp_buf SAVED_JMP;

void save() {}

void alpha() {
  fmt::println("alpha before");
  return save();
  fmt::println("alpha after");
}

void beta() {
  alpha();
}

int main() {
  return 0;
}
