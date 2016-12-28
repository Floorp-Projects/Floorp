/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/rtccertificate.h"

#include "webrtc/base/checks.h"

namespace rtc {

scoped_refptr<RTCCertificate> RTCCertificate::Create(
    scoped_ptr<SSLIdentity> identity) {
  return new RefCountedObject<RTCCertificate>(identity.release());
}

RTCCertificate::RTCCertificate(SSLIdentity* identity)
    : identity_(identity) {
  RTC_DCHECK(identity_);
}

RTCCertificate::~RTCCertificate() {
}

uint64_t RTCCertificate::Expires() const {
  int64_t expires = ssl_certificate().CertificateExpirationTime();
  if (expires != -1)
    return static_cast<uint64_t>(expires) * kNumMillisecsPerSec;
  // If the expiration time could not be retrieved return an expired timestamp.
  return 0;  // = 1970-01-01
}

bool RTCCertificate::HasExpired(uint64_t now) const {
  return Expires() <= now;
}

const SSLCertificate& RTCCertificate::ssl_certificate() const {
  return identity_->certificate();
}

}  // namespace rtc
