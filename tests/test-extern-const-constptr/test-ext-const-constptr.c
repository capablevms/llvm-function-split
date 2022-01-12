#include "test-ext-const-constptr.h"

const int val = 15;
const int *const whatever = &val;

int get_value(const int *whatever)
{
	return *whatever;
}