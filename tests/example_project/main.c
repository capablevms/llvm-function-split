#include "include/record.h"

/* A generic project composed of three modules:
 *   - Main operates on Records;
 *   - Record uses Internals;
 *   - Internals uses Isolated;
 *   - Isolated uses only its own functions/variables.
 */

int main() {
  const RECORD nice_record = {67789L, YELLOW, "Jim", "Debuis"};
  store_record(&nice_record);
  RECORD to_print = {5678L, RED, "Noah", "Gal"};
  pp_record(&to_print);

  return 0;
}
