#include <iostream>
#include "src/pkt.h"

using namespace boomerang;

int main() {
  std::ios::sync_with_stdio(false);

  std::cout << "pkt size: " << sizeof(Pkt) << "\n";
  std::cout << "pkt head size: " << sizeof(PktHead) << "\n";
  std::cout << "pkt content size: " << sizeof(Pkt) - sizeof(PktHead) - kMacSize << "\n";

  return 0;
}
