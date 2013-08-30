/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_RANDOM_ENCRYPTION_H_
#define SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_RANDOM_ENCRYPTION_H_

#include "webrtc/common_types.h"

// These algorithms attempt to create an uncrackable encryption
// scheme by completely disregarding the input data.
class RandomEncryption : public webrtc::Encryption {
 public:
  explicit RandomEncryption(unsigned int rand_seed);

  virtual void encrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void decrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void encrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void decrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

 private:
  // Generates some completely random data with roughly the right length.
  void GenerateRandomData(unsigned char* out_data, int bytes_in,
                          int* bytes_out);

  // Makes up a length within +- 50 of the original length, without
  // overstepping the contract for encrypt / decrypt.
  int MakeUpSimilarLength(int original_length);
};

#endif  // SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_RANDOM_ENCRYPTION_H_
