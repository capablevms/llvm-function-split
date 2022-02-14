typedef struct internal_details {
  struct {
    int zero;
    float one_point;
    double two_point_five;
  };
  char extra[30];
} INTERNAL_DETAILS;

typedef union internal_u {
  INTERNAL_DETAILS secret;
  int fake_secret;
  double corrupted_secret;
} SETTINGS;

void init_settings();