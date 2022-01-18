#include "pgcommon.h"
#include <stdio.h>

int main(void)
{
	int what = pglz_compress2(PROBLEMATIC_STRUCT_default);
	printf("Just trying to reproduce the bug. Value: %d\n", what);

	return 0;
}