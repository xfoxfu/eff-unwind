/**
 * Test case "generator"
 * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/generator/main.kk
 * https://github.com/maciejpirog/cpp-effects/blob/main/examples/generators.cpp
 * This test case TODO:
 */

// FIXME: support store resumption and call later

#include <memory>
#include <variant>
#include "eff-unwind.hpp"

struct Tree_Leaf;
struct Tree_Node;

typedef std::variant<Tree_Leaf, Tree_Node> Tree;
struct Tree_Leaf {};
struct Tree_Node {
  std::unique_ptr<Tree> left;
  int value;
  std::unique_ptr<Tree> right;
};

void run(uint64_t n) {}

int main() {
  for (int i = 0; i < 1'000'0; i++) {
    run(1000);
  }
  return 0;
}
