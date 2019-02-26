/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_protect.h"
#include "sslproto.h"
#include "tls_filter.h"

namespace nss_test {

static uint64_t FirstSeqno(bool dtls, uint16_t epoc) {
  if (dtls) {
    return static_cast<uint64_t>(epoc) << 48;
  }
  return 0;
}

TlsCipherSpec::TlsCipherSpec(bool dtls, uint16_t epoc)
    : dtls_(dtls),
      epoch_(epoc),
      in_seqno_(FirstSeqno(dtls, epoc)),
      out_seqno_(FirstSeqno(dtls, epoc)) {}

bool TlsCipherSpec::SetKeys(SSLCipherSuiteInfo* cipherinfo,
                            PK11SymKey* secret) {
  SSLAeadContext* ctx;
  SECStatus rv = SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3,
                              cipherinfo->cipherSuite, secret, "",
                              0,  // Use the default labels.
                              &ctx);
  if (rv != SECSuccess) {
    return false;
  }
  aead_.reset(ctx);
  return true;
}

bool TlsCipherSpec::Unprotect(const TlsRecordHeader& header,
                              const DataBuffer& ciphertext,
                              DataBuffer* plaintext) {
  if (aead_ == nullptr) {
    return false;
  }
  // Make space.
  plaintext->Allocate(ciphertext.len());

  auto header_bytes = header.header();
  unsigned int len;
  uint64_t seqno;
  if (dtls_) {
    seqno = header.sequence_number();
  } else {
    seqno = in_seqno_;
  }
  SECStatus rv =
      SSL_AeadDecrypt(aead_.get(), seqno, header_bytes.data(),
                      header_bytes.len(), ciphertext.data(), ciphertext.len(),
                      plaintext->data(), &len, plaintext->len());
  if (rv != SECSuccess) {
    return false;
  }

  RecordUnprotected(seqno);
  plaintext->Truncate(static_cast<size_t>(len));

  return true;
}

bool TlsCipherSpec::Protect(const TlsRecordHeader& header,
                            const DataBuffer& plaintext,
                            DataBuffer* ciphertext) {
  if (aead_ == nullptr) {
    return false;
  }
  // Make a padded buffer.
  ciphertext->Allocate(plaintext.len() +
                       32);  // Room for any plausible auth tag
  unsigned int len;

  DataBuffer header_bytes;
  (void)header.WriteHeader(&header_bytes, 0, plaintext.len() + 16);
  uint64_t seqno;
  if (dtls_) {
    seqno = header.sequence_number();
  } else {
    seqno = out_seqno_;
  }

  SECStatus rv =
      SSL_AeadEncrypt(aead_.get(), seqno, header_bytes.data(),
                      header_bytes.len(), plaintext.data(), plaintext.len(),
                      ciphertext->data(), &len, ciphertext->len());
  if (rv != SECSuccess) {
    return false;
  }

  RecordProtected();
  ciphertext->Truncate(len);

  return true;
}

}  // namespace nss_test
