#ifndef UTILS_H
#define UTILS_H

#include <math.h>

float wrap(float* a, float t, int s) {
  return a[((int)floor(t))%s];
}

#endif