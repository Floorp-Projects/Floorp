/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/libtest/include/bit_flip_encryption.h"

#include <cstdlib>

float NormalizedRand() {
  return static_cast<float>(rand()) /
         static_cast<float>(RAND_MAX);
}

BitFlipEncryption::BitFlipEncryption(unsigned int rand_seed,
                                     float flip_probability)
    : flip_probability_(flip_probability),
      flip_count_(0) {
  srand(rand_seed);
}

void BitFlipEncryption::FlipSomeBitsInData(const unsigned char* in_data,
                                           unsigned char* out_data,
                                           int bytes_in, int* bytes_out) {
  for (int i = 0; i < bytes_in; i++) {
    out_data[i] = in_data[i];

    if (NormalizedRand() < flip_probability_) {
      int bit_to_flip = rand() % 8;
      out_data[i] ^= 1 << bit_to_flip;
      flip_count_++;
    }
  }
  *bytes_out = bytes_in;
}
