// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef FAST_LOSSLESS_H
#define FAST_LOSSLESS_H
#include <stdlib.h>

size_t FastLosslessEncode(const unsigned char* rgba, size_t width,
                          size_t row_stride, size_t height, size_t nb_chans,
                          size_t bitdepth, int effort, unsigned char** output);

#endif
