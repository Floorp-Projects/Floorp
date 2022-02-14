// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <thread>

#include "fast_lossless.h"
#include "lodepng.h"
#include "pam-input.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s in.png out.jxl [effort] [num_reps]\n", argv[0]);
    return 1;
  }

  const char* in = argv[1];
  const char* out = argv[2];
  int effort = argc >= 4 ? atoi(argv[3]) : 2;
  size_t num_reps = argc >= 5 ? atoi(argv[4]) : 1;

  if (effort < 0 || effort > 127) {
    fprintf(
        stderr,
        "Effort should be between 0 and 127 (default is 2, more is slower)\n");
    return 1;
  }

  unsigned char* png;
  unsigned w, h;
  size_t nb_chans = 4, bitdepth = 8;

  unsigned error = lodepng_decode32_file(&png, &w, &h, in);

  size_t width = w, height = h;
  if (error && !DecodePAM(in, &png, &width, &height, &nb_chans, &bitdepth)) {
    fprintf(stderr, "lodepng error %u: %s\n", error, lodepng_error_text(error));
    return 1;
  }

  size_t encoded_size = 0;
  unsigned char* encoded = nullptr;
  size_t stride = width * nb_chans * (bitdepth > 8 ? 2 : 1);

  auto start = std::chrono::high_resolution_clock::now();
  for (size_t _ = 0; _ < num_reps; _++) {
    free(encoded);
    encoded_size = FastLosslessEncode(png, width, stride, height, nb_chans,
                                      bitdepth, effort, &encoded);
  }
  auto stop = std::chrono::high_resolution_clock::now();
  if (num_reps > 1) {
    float us =
        std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
            .count();
    size_t pixels = size_t{width} * size_t{height} * num_reps;
    float mps = pixels / us;
    fprintf(stderr, "%10.3f MP/s\n", mps);
    fprintf(stderr, "%10.3f bits/pixel\n",
            encoded_size * 8.0 / float(width) / float(height));
  }

  FILE* o = fopen(out, "wb");
  if (!o) {
    fprintf(stderr, "error opening %s: %s\n", out, strerror(errno));
    return 1;
  }
  if (fwrite(encoded, 1, encoded_size, o) != encoded_size) {
    fprintf(stderr, "error writing to %s: %s\n", out, strerror(errno));
  }
  fclose(o);
}
