/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/libtest/include/random_encryption.h"

#include <math.h>
#include <stdlib.h>

#include <algorithm>

#include "webrtc/video_engine/vie_defines.h"

static int Saturate(int value, int min, int max) {
  return std::min(std::max(value, min), max);
}

RandomEncryption::RandomEncryption(unsigned int rand_seed) {
  srand(rand_seed);
}

// Generates some completely random data with roughly the right length.
void RandomEncryption::GenerateRandomData(unsigned char* out_data, int bytes_in,
                                          int* bytes_out) {
  int out_length = MakeUpSimilarLength(bytes_in);
  for (int i = 0; i < out_length; i++) {
    // The modulo will skew the random distribution a bit, but I think it
    // will be random enough.
    out_data[i] = static_cast<unsigned char>(rand() % 256);
  }
  *bytes_out = out_length;
}

// Makes up a length within +- 50 of the original length, without
// overstepping the contract for encrypt / decrypt.
int RandomEncryption::MakeUpSimilarLength(int original_length) {
  int sign = rand() - RAND_MAX / 2;
  int length = original_length + sign * rand() % 50;

  return Saturate(length, 0, static_cast<int>(webrtc::kViEMaxMtu));
}
