/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

void aom_plane_add_noise_c(uint8_t *start, char *noise, char blackclamp[16],
                           char whiteclamp[16], char bothclamp[16],
                           unsigned int width, unsigned int height, int pitch) {
  unsigned int i, j;

  for (i = 0; i < height; ++i) {
    uint8_t *pos = start + i * pitch;
    char *ref = (char *)(noise + (rand() & 0xff));  // NOLINT

    for (j = 0; j < width; ++j) {
      int v = pos[j];

      v = clamp(v - blackclamp[0], 0, 255);
      v = clamp(v + bothclamp[0], 0, 255);
      v = clamp(v - whiteclamp[0], 0, 255);

      pos[j] = v + ref[j];
    }
  }
}

static double gaussian(double sigma, double mu, double x) {
  return 1 / (sigma * sqrt(2.0 * 3.14159265)) *
         (exp(-(x - mu) * (x - mu) / (2 * sigma * sigma)));
}

int aom_setup_noise(double sigma, int size, char *noise) {
  char char_dist[256];
  int next = 0, i, j;

  // set up a 256 entry lookup that matches gaussian distribution
  for (i = -32; i < 32; ++i) {
    const int a_i = (int)(0.5 + 256 * gaussian(sigma, 0, i));
    if (a_i) {
      for (j = 0; j < a_i; ++j) {
        char_dist[next + j] = (char)i;
      }
      next = next + j;
    }
  }

  // Rounding error - might mean we have less than 256.
  for (; next < 256; ++next) {
    char_dist[next] = 0;
  }

  for (i = 0; i < size; ++i) {
    noise[i] = char_dist[rand() & 0xff];  // NOLINT
  }

  // Returns the highest non 0 value used in distribution.
  return -char_dist[0];
}
