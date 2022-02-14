#include "include/record.h"
#include "include/internals.h"
#include <stdio.h>

static const RECORD PLACEHOLDER = {1000L, BLUE, "John", "Martins"};
const RECORD *const GLOBAL_DEFAULT_RECORD = &PLACEHOLDER;

int convert_record_id(long to_convert, int conversion_factor) {
  return (int)(to_convert * conversion_factor);
}

void pp_record(RECORD *rec) {
  init_settings();
  printf("ID: %ld\n", rec->numeric_id);
  printf("LABEL: %d\n", rec->label);
  printf("FIRST NAME: %s\n", rec->first_name);
  printf("LAST NAME: %s\n", rec->last_name);
}

int store_record(const RECORD *rec) {
  if (rec == NULL) {
    rec = GLOBAL_DEFAULT_RECORD;
    printf("Storing default record to file...\n");
  } else {
    convert_record_id(rec->numeric_id, 3);
    printf("Storing record %ld to file...\n", rec->numeric_id);
  }
  return write_record_to_file(rec);
}

static int write_record_to_file(const RECORD *rec) {
  FILE *fptr = fopen("record.ext", "w");
  if (fptr != NULL) {
    fprintf(fptr, "%ld, %d, %s, %s\n", rec->numeric_id, rec->label,
            rec->first_name, rec->last_name);
    fflush(stdout);
  }
  return fclose(fptr);
}