/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_protect.h"
#include "tls_filter.h"

namespace nss_test {

AeadCipher::~AeadCipher() {
  if (key_) {
    PK11_FreeSymKey(key_);
  }
}

bool AeadCipher::Init(PK11SymKey *key, const uint8_t *iv) {
  key_ = PK11_ReferenceSymKey(key);
  if (!key_) return false;

  memcpy(iv_, iv, sizeof(iv_));
  return true;
}

void AeadCipher::FormatNonce(uint64_t seq, uint8_t *nonce) {
  memcpy(nonce, iv_, 12);

  for (size_t i = 0; i < 8; ++i) {
    nonce[12 - (i + 1)] ^= seq & 0xff;
    seq >>= 8;
  }

  DataBuffer d(nonce, 12);
}

bool AeadCipher::AeadInner(bool decrypt, void *params, size_t param_length,
                           const uint8_t *in, size_t inlen, uint8_t *out,
                           size_t *outlen, size_t maxlen) {
  SECStatus rv;
  unsigned int uoutlen = 0;
  SECItem param = {
      siBuffer, static_cast<unsigned char *>(params),
      static_cast<unsigned int>(param_length),
  };

  if (decrypt) {
    rv = PK11_Decrypt(key_, mech_, &param, out, &uoutlen, maxlen, in, inlen);
  } else {
    rv = PK11_Encrypt(key_, mech_, &param, out, &uoutlen, maxlen, in, inlen);
  }
  *outlen = (int)uoutlen;

  return rv == SECSuccess;
}

bool AeadCipherAesGcm::Aead(bool decrypt, const uint8_t *hdr, size_t hdr_len,
                            uint64_t seq, const uint8_t *in, size_t inlen,
                            uint8_t *out, size_t *outlen, size_t maxlen) {
  CK_GCM_PARAMS aeadParams;
  unsigned char nonce[12];

  memset(&aeadParams, 0, sizeof(aeadParams));
  aeadParams.pIv = nonce;
  aeadParams.ulIvLen = sizeof(nonce);
  aeadParams.pAAD = const_cast<uint8_t *>(hdr);
  aeadParams.ulAADLen = hdr_len;
  aeadParams.ulTagBits = 128;

  FormatNonce(seq, nonce);
  return AeadInner(decrypt, (unsigned char *)&aeadParams, sizeof(aeadParams),
                   in, inlen, out, outlen, maxlen);
}

bool AeadCipherChacha20Poly1305::Aead(bool decrypt, const uint8_t *hdr,
                                      size_t hdr_len, uint64_t seq,
                                      const uint8_t *in, size_t inlen,
                                      uint8_t *out, size_t *outlen,
                                      size_t maxlen) {
  CK_NSS_AEAD_PARAMS aeadParams;
  unsigned char nonce[12];

  memset(&aeadParams, 0, sizeof(aeadParams));
  aeadParams.pNonce = nonce;
  aeadParams.ulNonceLen = sizeof(nonce);
  aeadParams.pAAD = const_cast<uint8_t *>(hdr);
  aeadParams.ulAADLen = hdr_len;
  aeadParams.ulTagLen = 16;

  FormatNonce(seq, nonce);
  return AeadInner(decrypt, (unsigned char *)&aeadParams, sizeof(aeadParams),
                   in, inlen, out, outlen, maxlen);
}

bool TlsCipherSpec::Init(uint16_t epoc, SSLCipherAlgorithm cipher,
                         PK11SymKey *key, const uint8_t *iv) {
  epoch_ = epoc;
  switch (cipher) {
    case ssl_calg_aes_gcm:
      aead_.reset(new AeadCipherAesGcm());
      break;
    case ssl_calg_chacha20:
      aead_.reset(new AeadCipherChacha20Poly1305());
      break;
    default:
      return false;
  }

  return aead_->Init(key, iv);
}

bool TlsCipherSpec::Unprotect(const TlsRecordHeader &header,
                              const DataBuffer &ciphertext,
                              DataBuffer *plaintext) {
  // Make space.
  plaintext->Allocate(ciphertext.len());

  auto header_bytes = header.header();
  size_t len;
  bool ret =
      aead_->Aead(true, header_bytes.data(), header_bytes.len(),
                  header.sequence_number(), ciphertext.data(), ciphertext.len(),
                  plaintext->data(), &len, plaintext->len());
  if (!ret) return false;

  plaintext->Truncate(len);

  return true;
}

bool TlsCipherSpec::Protect(const TlsRecordHeader &header,
                            const DataBuffer &plaintext,
                            DataBuffer *ciphertext) {
  // Make a padded buffer.

  ciphertext->Allocate(plaintext.len() +
                       32);  // Room for any plausible auth tag
  size_t len;

  DataBuffer header_bytes;
  (void)header.WriteHeader(&header_bytes, 0, plaintext.len() + 16);
  bool ret =
      aead_->Aead(false, header_bytes.data(), header_bytes.len(),
                  header.sequence_number(), plaintext.data(), plaintext.len(),
                  ciphertext->data(), &len, ciphertext->len());
  if (!ret) return false;
  ciphertext->Truncate(len);

  return true;
}

}  // namespace nss_test
