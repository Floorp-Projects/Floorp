/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_protection_h_
#define tls_protection_h_

#include <cstdint>
#include <memory>

#include "pk11pub.h"
#include "sslt.h"
#include "sslexp.h"

#include "databuffer.h"
#include "scoped_ptrs_ssl.h"

namespace nss_test {
class TlsRecordHeader;

// Our analog of ssl3CipherSpec
class TlsCipherSpec {
 public:
  TlsCipherSpec(bool dtls, uint16_t epoc);
  bool SetKeys(SSLCipherSuiteInfo* cipherinfo, PK11SymKey* secret);

  bool Protect(const TlsRecordHeader& header, const DataBuffer& plaintext,
               DataBuffer* ciphertext, TlsRecordHeader* out_header);
  bool Unprotect(const TlsRecordHeader& header, const DataBuffer& ciphertext,
                 DataBuffer* plaintext, TlsRecordHeader* out_header);

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
  ScopedSSLAeadContext aead_;
  ScopedSSLMaskingContext mask_;
};

}  // namespace nss_test

#endif
