/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_protect.h"
#include "tls_filter.h"

// Do this to avoid having to re-implement HKDF.
#include "tls13hkdf.h"

namespace nss_test {

AeadCipher::~AeadCipher() {
  if (key_) {
    PK11_FreeSymKey(key_);
  }
}

bool AeadCipher::Init(PK11SymKey* key, const uint8_t* iv) {
  key_ = key;
  if (!key_) return false;

  memcpy(iv_, iv, sizeof(iv_));
  if (g_ssl_gtest_verbose) {
    EXPECT_EQ(SECSuccess, PK11_ExtractKeyValue(key_));
    SECItem* raw_key = PK11_GetKeyData(key_);
    std::cerr << "key: " << DataBuffer(raw_key->data, raw_key->len)
              << std::endl;
    std::cerr << "iv: " << DataBuffer(iv_, 12) << std::endl;
  }
  return true;
}

void AeadCipher::FormatNonce(uint64_t seq, uint8_t* nonce) {
  memcpy(nonce, iv_, 12);

  for (size_t i = 0; i < 8; ++i) {
    nonce[12 - (i + 1)] ^= seq & 0xff;
    seq >>= 8;
  }
}

bool AeadCipher::AeadInner(bool decrypt, void* params, size_t param_length,
                           const uint8_t* in, size_t inlen, uint8_t* out,
                           size_t* outlen, size_t maxlen) {
  SECStatus rv;
  unsigned int uoutlen = 0;
  SECItem param = {
      siBuffer, static_cast<unsigned char*>(params),
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

bool AeadCipherAesGcm::Aead(bool decrypt, const uint8_t* hdr, size_t hdr_len,
                            uint64_t seq, const uint8_t* in, size_t inlen,
                            uint8_t* out, size_t* outlen, size_t maxlen) {
  CK_GCM_PARAMS aeadParams;
  unsigned char nonce[12];

  memset(&aeadParams, 0, sizeof(aeadParams));
  aeadParams.pIv = nonce;
  aeadParams.ulIvLen = sizeof(nonce);
  aeadParams.pAAD = const_cast<uint8_t*>(hdr);
  aeadParams.ulAADLen = hdr_len;
  aeadParams.ulTagBits = 128;

  FormatNonce(seq, nonce);
  return AeadInner(decrypt, (unsigned char*)&aeadParams, sizeof(aeadParams), in,
                   inlen, out, outlen, maxlen);
}

bool AeadCipherChacha20Poly1305::Aead(bool decrypt, const uint8_t* hdr,
                                      size_t hdr_len, uint64_t seq,
                                      const uint8_t* in, size_t inlen,
                                      uint8_t* out, size_t* outlen,
                                      size_t maxlen) {
  CK_NSS_AEAD_PARAMS aeadParams;
  unsigned char nonce[12];

  memset(&aeadParams, 0, sizeof(aeadParams));
  aeadParams.pNonce = nonce;
  aeadParams.ulNonceLen = sizeof(nonce);
  aeadParams.pAAD = const_cast<uint8_t*>(hdr);
  aeadParams.ulAADLen = hdr_len;
  aeadParams.ulTagLen = 16;

  FormatNonce(seq, nonce);
  return AeadInner(decrypt, (unsigned char*)&aeadParams, sizeof(aeadParams), in,
                   inlen, out, outlen, maxlen);
}

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
  CK_MECHANISM_TYPE mech;
  switch (cipherinfo->symCipher) {
    case ssl_calg_aes_gcm:
      aead_.reset(new AeadCipherAesGcm());
      mech = CKM_AES_GCM;
      break;
    case ssl_calg_chacha20:
      aead_.reset(new AeadCipherChacha20Poly1305());
      mech = CKM_NSS_CHACHA20_POLY1305;
      break;
    default:
      return false;
  }

  PK11SymKey* key;
  const std::string kPurposeKey = "key";
  SECStatus rv = tls13_HkdfExpandLabel(
      secret, cipherinfo->kdfHash, NULL, 0, kPurposeKey.c_str(),
      kPurposeKey.length(), mech, cipherinfo->symKeyBits / 8, &key);
  if (rv != SECSuccess) {
    ADD_FAILURE() << "unable to derive key for epoch " << epoch_;
    return false;
  }

  // No constant for IV length, but everything we know of uses 12.
  uint8_t iv[12];
  const std::string kPurposeIv = "iv";
  rv = tls13_HkdfExpandLabelRaw(secret, cipherinfo->kdfHash, NULL, 0,
                                kPurposeIv.c_str(), kPurposeIv.length(), iv,
                                sizeof(iv));
  if (rv != SECSuccess) {
    ADD_FAILURE() << "unable to derive IV for epoch " << epoch_;
    return false;
  }

  return aead_->Init(key, iv);
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
  size_t len;
  uint64_t seqno;
  if (dtls_) {
    seqno = header.sequence_number();
  } else {
    seqno = in_seqno_;
  }
  bool ret = aead_->Aead(true, header_bytes.data(), header_bytes.len(), seqno,
                         ciphertext.data(), ciphertext.len(), plaintext->data(),
                         &len, plaintext->len());
  if (!ret) {
    return false;
  }

  RecordUnprotected(seqno);
  plaintext->Truncate(len);

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
  size_t len;

  DataBuffer header_bytes;
  (void)header.WriteHeader(&header_bytes, 0, plaintext.len() + 16);
  uint64_t seqno;
  if (dtls_) {
    seqno = header.sequence_number();
  } else {
    seqno = out_seqno_;
  }

  bool ret = aead_->Aead(false, header_bytes.data(), header_bytes.len(), seqno,
                         plaintext.data(), plaintext.len(), ciphertext->data(),
                         &len, ciphertext->len());
  if (!ret) {
    return false;
  }

  RecordProtected();
  ciphertext->Truncate(len);

  return true;
}

}  // namespace nss_test
