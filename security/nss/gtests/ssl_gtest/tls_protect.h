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

  bool Init(PK11SymKey* key, const uint8_t* iv);
  virtual bool Aead(bool decrypt, const uint8_t* hdr, size_t hdr_len,
                    uint64_t seq, const uint8_t* in, size_t inlen, uint8_t* out,
                    size_t* outlen, size_t maxlen) = 0;

 protected:
  void FormatNonce(uint64_t seq, uint8_t* nonce);
  bool AeadInner(bool decrypt, void* params, size_t param_length,
                 const uint8_t* in, size_t inlen, uint8_t* out, size_t* outlen,
                 size_t maxlen);

  CK_MECHANISM_TYPE mech_;
  PK11SymKey* key_;
  uint8_t iv_[12];
};

class AeadCipherChacha20Poly1305 : public AeadCipher {
 public:
  AeadCipherChacha20Poly1305() : AeadCipher(CKM_NSS_CHACHA20_POLY1305) {}

 protected:
  bool Aead(bool decrypt, const uint8_t* hdr, size_t hdr_len, uint64_t seq,
            const uint8_t* in, size_t inlen, uint8_t* out, size_t* outlen,
            size_t maxlen);
};

class AeadCipherAesGcm : public AeadCipher {
 public:
  AeadCipherAesGcm() : AeadCipher(CKM_AES_GCM) {}

 protected:
  bool Aead(bool decrypt, const uint8_t* hdr, size_t hdr_len, uint64_t seq,
            const uint8_t* in, size_t inlen, uint8_t* out, size_t* outlen,
            size_t maxlen);
};

// Our analog of ssl3CipherSpec
class TlsCipherSpec {
 public:
  TlsCipherSpec(bool dtls, uint16_t epoc);
  bool SetKeys(SSLCipherSuiteInfo* cipherinfo, PK11SymKey* secret);

  bool Protect(const TlsRecordHeader& header, const DataBuffer& plaintext,
               DataBuffer* ciphertext);
  bool Unprotect(const TlsRecordHeader& header, const DataBuffer& ciphertext,
                 DataBuffer* plaintext);

  uint16_t epoch() const { return epoch_; }
  uint64_t next_in_seqno() const { return in_seqno_; }
  void RecordUnprotected(uint64_t seqno) {
    // Reordering happens, so don't let this go backwards.
    in_seqno_ = (std::max)(in_seqno_, seqno + 1);
  }
  uint64_t next_out_seqno() { return out_seqno_; }
  void RecordProtected() { out_seqno_++; }

  void RecordDropped() { record_dropped_ = true; }
  bool record_dropped() const { return record_dropped_; }

  bool is_protected() const { return aead_ != nullptr; }

 private:
  bool dtls_;
  uint16_t epoch_;
  uint64_t in_seqno_;
  uint64_t out_seqno_;
  bool record_dropped_ = false;
  std::unique_ptr<AeadCipher> aead_;
};

}  // namespace nss_test

#endif
