#include "include/isolated.h"
#include <stdio.h>

static float ALONE = 5.5;

bool is_alone() { return ALONE == 5.5; }

void set_alone(float value) { ALONE = value; }

void pp() {
  if (is_alone()) {
    printf("Pretty Printing is nice: %f \n", ALONE);
  } else {
    printf("Pretty Printing is always nice: %f \n", (ALONE - 0.44));
  }
}