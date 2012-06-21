/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_BIT_FLIP_ENCRYPTION_H_
#define SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_BIT_FLIP_ENCRYPTION_H_

#include "video_engine/include/vie_encryption.h"

// This encryption scheme will randomly flip bits every now and then in the
// input data.
class BitFlipEncryption : public webrtc::Encryption {
 public:
  // Args:
  //   rand_seed: the seed to initialize the test's random generator with.
  //   flip_probability: A number [0, 1] which is the percentage chance a bit
  //       gets flipped in a particular byte.
  BitFlipEncryption(unsigned int rand_seed, float flip_probability);

  virtual void encrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    FlipSomeBitsInData(in_data, out_data, bytes_in, bytes_out);
  }

  virtual void decrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    FlipSomeBitsInData(in_data, out_data, bytes_in, bytes_out);
  }

  virtual void encrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    FlipSomeBitsInData(in_data, out_data, bytes_in, bytes_out);
  }

  virtual void decrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    FlipSomeBitsInData(in_data, out_data, bytes_in, bytes_out);
  }

  int64_t flip_count() const { return flip_count_; }

 private:
  // The flip probability ([0, 1]).
  float flip_probability_;
  // The number of bits we've flipped so far.
  int64_t flip_count_;

  // Flips some bits in the data at random.
  void FlipSomeBitsInData(const unsigned char *in_data, unsigned char* out_data,
                          int bytes_in, int* bytes_out);
};

#endif  // SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_BIT_FLIP_ENCRYPTION_H_
