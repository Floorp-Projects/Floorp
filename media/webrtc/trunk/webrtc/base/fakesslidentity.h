/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FAKESSLIDENTITY_H_
#define WEBRTC_BASE_FAKESSLIDENTITY_H_

#include <algorithm>
#include <vector>

#include "webrtc/base/messagedigest.h"
#include "webrtc/base/sslidentity.h"

namespace rtc {

class FakeSSLCertificate : public rtc::SSLCertificate {
 public:
  // SHA-1 is the default digest algorithm because it is available in all build
  // configurations used for unit testing.
  explicit FakeSSLCertificate(const std::string& data)
      : data_(data), digest_algorithm_(DIGEST_SHA_1) {}
  explicit FakeSSLCertificate(const std::vector<std::string>& certs)
      : data_(certs.front()), digest_algorithm_(DIGEST_SHA_1) {
    std::vector<std::string>::const_iterator it;
    // Skip certs[0].
    for (it = certs.begin() + 1; it != certs.end(); ++it) {
      certs_.push_back(FakeSSLCertificate(*it));
    }
  }
  virtual FakeSSLCertificate* GetReference() const {
    return new FakeSSLCertificate(*this);
  }
  virtual std::string ToPEMString() const {
    return data_;
  }
  virtual void ToDER(Buffer* der_buffer) const {
    std::string der_string;
    VERIFY(SSLIdentity::PemToDer(kPemTypeCertificate, data_, &der_string));
    der_buffer->SetData(der_string.c_str(), der_string.size());
  }
  void set_digest_algorithm(const std::string& algorithm) {
    digest_algorithm_ = algorithm;
  }
  virtual bool GetSignatureDigestAlgorithm(std::string* algorithm) const {
    *algorithm = digest_algorithm_;
    return true;
  }
  virtual bool ComputeDigest(const std::string& algorithm,
                             unsigned char* digest,
                             size_t size,
                             size_t* length) const {
    *length = rtc::ComputeDigest(algorithm, data_.c_str(), data_.size(),
                                       digest, size);
    return (*length != 0);
  }
  virtual bool GetChain(SSLCertChain** chain) const {
    if (certs_.empty())
      return false;
    std::vector<SSLCertificate*> new_certs(certs_.size());
    std::transform(certs_.begin(), certs_.end(), new_certs.begin(), DupCert);
    *chain = new SSLCertChain(new_certs);
    std::for_each(new_certs.begin(), new_certs.end(), DeleteCert);
    return true;
  }

 private:
  static FakeSSLCertificate* DupCert(FakeSSLCertificate cert) {
    return cert.GetReference();
  }
  static void DeleteCert(SSLCertificate* cert) { delete cert; }
  std::string data_;
  std::vector<FakeSSLCertificate> certs_;
  std::string digest_algorithm_;
};

class FakeSSLIdentity : public rtc::SSLIdentity {
 public:
  explicit FakeSSLIdentity(const std::string& data) : cert_(data) {}
  explicit FakeSSLIdentity(const FakeSSLCertificate& cert) : cert_(cert) {}
  virtual FakeSSLIdentity* GetReference() const {
    return new FakeSSLIdentity(*this);
  }
  virtual const FakeSSLCertificate& certificate() const { return cert_; }
 private:
  FakeSSLCertificate cert_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FAKESSLIDENTITY_H_
