#include "include/internals.h"
#include "include/isolated.h"
#include <stdio.h>

INTERNAL_DETAILS more_details = {0, 1.0, 2.5, "Something else here..."};

void static my_business_fun(int a, int b, float c);

void init_settings() {
  SETTINGS weekly_settings = {};
  weekly_settings.secret = more_details;
  pp();
  my_business_fun(23, 45, 21.28);
  set_alone(9.9);
  pp();
}

void static my_business_fun(int a, int b, float c) {
  printf("Internals: %f\n", (float)a * b * c);
}
