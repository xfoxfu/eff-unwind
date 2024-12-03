// /**
//  * Test case "generator"
//  * https://github.com/effect-handlers/effect-handlers-bench/blob/main/benchmarks/koka/generator/main.kk
//  * https://github.com/maciejpirog/cpp-effects/blob/main/examples/generators.cpp
//  * This test case TODO:
//  */

// // FIXME: support store resumption and call later

// #include <iostream>
// #include <memory>
// #include <variant>
// #include "eff-unwind.hpp"

// struct Tree_Leaf;
// struct Tree_Node;

// typedef std::variant<Tree_Leaf, Tree_Node> Tree;
// struct Tree_Leaf {};
// struct Tree_Node {
//   std::unique_ptr<Tree> left;
//   int value;
//   std::unique_ptr<Tree> right;
// };

// int run(uint64_t n) {
//   return 0;
// }

// int main(int, char** argv) {
//   auto size = std::stoi(argv[1]);
//   auto value = run(size);
//   std::cout << value << std::endl;
// #ifndef NDEBUG
//   if (size == 40000000) {
//     assert(value == 67108837);
//   }
// #endif
//   return 0;
// }
int main() {
  return 0;
}
