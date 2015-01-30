/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_SHA1DIGEST_H_
#define WEBRTC_BASE_SHA1DIGEST_H_

#include "webrtc/base/messagedigest.h"
#include "webrtc/base/sha1.h"

namespace rtc {

// A simple wrapper for our SHA-1 implementation.
class Sha1Digest : public MessageDigest {
 public:
  enum { kSize = SHA1_DIGEST_SIZE };
  Sha1Digest() {
    SHA1Init(&ctx_);
  }
  virtual size_t Size() const {
    return kSize;
  }
  virtual void Update(const void* buf, size_t len) {
    SHA1Update(&ctx_, static_cast<const uint8*>(buf), len);
  }
  virtual size_t Finish(void* buf, size_t len) {
    if (len < kSize) {
      return 0;
    }
    SHA1Final(&ctx_, static_cast<uint8*>(buf));
    SHA1Init(&ctx_);  // Reset for next use.
    return kSize;
  }

 private:
  SHA1_CTX ctx_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_SHA1DIGEST_H_
