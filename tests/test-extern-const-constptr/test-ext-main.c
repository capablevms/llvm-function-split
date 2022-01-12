// Test that an "extern" variable of type "const . *const"
// is preserved during the splitting phase

// clang-format off
/*
run:
  stdout:
    Value: 15
*/
// clang-format on

#include "test-ext-const-constptr.h"
#include <stdio.h>

int main(void)
{
	int what = get_value(whatever);
	printf("Value: %d\n", what);

	return 0;
}