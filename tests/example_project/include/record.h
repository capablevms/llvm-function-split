typedef struct record {
  long numeric_id;
  enum record_label { RED, BLUE, GRAY, YELLOW } label;
  char first_name[12];
  char last_name[12];
} RECORD;

int convert_record_id(long to_convert, int conversion_factor);
void pp_record(RECORD *rec);
static int write_record_to_file(const RECORD *rec);

extern const RECORD *const GLOBAL_DEFAULT_RECORD;
extern int store_record(const RECORD *rec);