#include "pgcommon.h"

static const PROBLEMATIC_STRUCT problematic_data = {2};
const PROBLEMATIC_STRUCT *const PROBLEMATIC_STRUCT_default = &problematic_data;

int pglz_compress2(const PROBLEMATIC_STRUCT *prob)
{
	prob = PROBLEMATIC_STRUCT_default;
	return prob->two;
}