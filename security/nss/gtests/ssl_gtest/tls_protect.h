/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_protection_h_
#define tls_protection_h_

#include <cstdint>
#include <memory>

#include "databuffer.h"
#include "pk11pub.h"
#include "sslt.h"

namespace nss_test {
class TlsRecordHeader;

class AeadCipher {
 public:
  AeadCipher(CK_MECHANISM_TYPE mech) : mech_(mech), key_(nullptr) {}
  virtual ~AeadCipher();

  bool Init(PK11SymKey *key, const uint8_t *iv);
  virtual bool Aead(bool decrypt, const uint8_t *hdr, size_t hdr_len,
                    uint64_t seq, const uint8_t *in, size_t inlen, uint8_t *out,
                    size_t *outlen, size_t maxlen) = 0;

 protected:
  void FormatNonce(uint64_t seq, uint8_t *nonce);
  bool AeadInner(bool decrypt, void *params, size_t param_length,
                 const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen,
                 size_t maxlen);

  CK_MECHANISM_TYPE mech_;
  PK11SymKey *key_;
  uint8_t iv_[12];
};

class AeadCipherChacha20Poly1305 : public AeadCipher {
 public:
  AeadCipherChacha20Poly1305() : AeadCipher(CKM_NSS_CHACHA20_POLY1305) {}

 protected:
  bool Aead(bool decrypt, const uint8_t *hdr, size_t hdr_len, uint64_t seq,
            const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen,
            size_t maxlen);
};

class AeadCipherAesGcm : public AeadCipher {
 public:
  AeadCipherAesGcm() : AeadCipher(CKM_AES_GCM) {}

 protected:
  bool Aead(bool decrypt, const uint8_t *hdr, size_t hdr_len, uint64_t seq,
            const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen,
            size_t maxlen);
};

// Our analog of ssl3CipherSpec
class TlsCipherSpec {
 public:
  TlsCipherSpec() : epoch_(0), aead_() {}

  bool Init(uint16_t epoch, SSLCipherAlgorithm cipher, PK11SymKey *key,
            const uint8_t *iv);

  bool Protect(const TlsRecordHeader &header, const DataBuffer &plaintext,
               DataBuffer *ciphertext);
  bool Unprotect(const TlsRecordHeader &header, const DataBuffer &ciphertext,
                 DataBuffer *plaintext);
  uint16_t epoch() const { return epoch_; }

 private:
  uint16_t epoch_;
  std::unique_ptr<AeadCipher> aead_;
};

}  // namespace nss_test

#endif
