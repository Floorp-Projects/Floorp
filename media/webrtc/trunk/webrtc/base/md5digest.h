/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_MD5DIGEST_H_
#define WEBRTC_BASE_MD5DIGEST_H_

#include "webrtc/base/md5.h"
#include "webrtc/base/messagedigest.h"

namespace rtc {

// A simple wrapper for our MD5 implementation.
class Md5Digest : public MessageDigest {
 public:
  enum { kSize = 16 };
  Md5Digest() {
    MD5Init(&ctx_);
  }
  virtual size_t Size() const {
    return kSize;
  }
  virtual void Update(const void* buf, size_t len) {
    MD5Update(&ctx_, static_cast<const uint8*>(buf), len);
  }
  virtual size_t Finish(void* buf, size_t len) {
    if (len < kSize) {
      return 0;
    }
    MD5Final(&ctx_, static_cast<uint8*>(buf));
    MD5Init(&ctx_);  // Reset for next use.
    return kSize;
  }
 private:
  MD5_CTX ctx_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_MD5DIGEST_H_
