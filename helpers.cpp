#include "helpers.h"

void set_time_diff(int diff, int& idx_10h, int& idx_01h) {
  int tmp = 10 * idx_10h + idx_01h;
  tmp += diff;
  if (tmp >= 24) {
    tmp %= 24;
  }
  if (tmp < 0) {
    tmp += 24;
  }
  idx_10h = tmp / 10;
  idx_01h = tmp % 10;
}