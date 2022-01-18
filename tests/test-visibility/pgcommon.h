typedef struct PROBLEMATIC_STRUCT
{
	int two;
} PROBLEMATIC_STRUCT;

extern const PROBLEMATIC_STRUCT *const PROBLEMATIC_STRUCT_default;

// Global function declaration
extern int pglz_compress2(const PROBLEMATIC_STRUCT *prob);