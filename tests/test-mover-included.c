// clang-format off
/*
run:
  status: 24
  stdout: 14 10
*/
// clang-format on

#include <stdio.h>

__attribute__((visibility("hidden"))) int a() { return 9; }

int b() { return a() + 5; }

int cc() { return a() + 1; }

int main() {
  printf("%d %d\n", b(), cc());
  return b() + cc();
}
