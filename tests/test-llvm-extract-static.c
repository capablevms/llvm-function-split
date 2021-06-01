// clang-format off
/*
run:
  stdout: 113 110
*/
// clang-format on

#include <stdio.h>

static int data = 99;

int a() { return 9; }

int b() { return a() + data + 5; }

int cc() {
  data += 1;
  return a() + data + 1;
}

int main() { printf("%d %d\n", b(), cc()); }
