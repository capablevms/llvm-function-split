// clang-format off
/*
run:
  stdout: 14 10
*/
// clang-format on

#include <stdio.h>

int a()
{
	return 9;
}

int b()
{
	return a() + 5;
}

int cc()
{
	return a() + 1;
}

int main()
{
	printf("%d %d\n", b(), cc());
}
