// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FDRM_CRYPTO_FX_CRYPT_H_
#define CORE_FDRM_CRYPTO_FX_CRYPT_H_

#include "core/fxcrt/fx_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t x;
  int32_t y;
  int32_t m[256];
} CRYPT_rc4_context;

typedef struct {
  uint32_t total[2];
  uint32_t state[4];
  uint8_t buffer[64];
} CRYPT_md5_context;

typedef struct {
  unsigned int h[5];
  unsigned char block[64];
  int blkused;
  unsigned int lenhi;
  unsigned int lenlo;
} CRYPT_sha1_context;

typedef struct {
  uint32_t total[2];
  uint32_t state[8];
  uint8_t buffer[64];
} CRYPT_sha256_context;

typedef struct {
  uint64_t total[2];
  uint64_t state[8];
  uint8_t buffer[128];
} CRYPT_sha384_context;

void CRYPT_ArcFourCryptBlock(uint8_t* data,
                             uint32_t size,
                             const uint8_t* key,
                             uint32_t keylen);
void CRYPT_ArcFourSetup(CRYPT_rc4_context* context,
                        const uint8_t* key,
                        uint32_t length);
void CRYPT_ArcFourCrypt(CRYPT_rc4_context* context,
                        uint8_t* data,
                        uint32_t size);

void CRYPT_AESSetKey(void* context,
                     uint32_t blocklen,
                     const uint8_t* key,
                     uint32_t keylen,
                     bool bEncrypt);
void CRYPT_AESSetIV(void* context, const uint8_t* iv);
void CRYPT_AESDecrypt(void* context,
                      uint8_t* dest,
                      const uint8_t* src,
                      uint32_t size);
void CRYPT_AESEncrypt(void* context,
                      uint8_t* dest,
                      const uint8_t* src,
                      uint32_t size);

void CRYPT_MD5Start(CRYPT_md5_context* context);
void CRYPT_MD5Update(CRYPT_md5_context* context,
                     const uint8_t* data,
                     uint32_t size);
void CRYPT_MD5Finish(CRYPT_md5_context* context, uint8_t digest[16]);
void CRYPT_MD5Generate(const uint8_t* data, uint32_t size, uint8_t digest[16]);

void CRYPT_SHA1Start(CRYPT_sha1_context* context);
void CRYPT_SHA1Update(CRYPT_sha1_context* context,
                      const uint8_t* data,
                      uint32_t size);
void CRYPT_SHA1Finish(CRYPT_sha1_context* context, uint8_t digest[20]);
void CRYPT_SHA1Generate(const uint8_t* data, uint32_t size, uint8_t digest[20]);

void CRYPT_SHA256Start(CRYPT_sha256_context* context);
void CRYPT_SHA256Update(CRYPT_sha256_context* context,
                        const uint8_t* data,
                        uint32_t size);
void CRYPT_SHA256Finish(CRYPT_sha256_context* context, uint8_t digest[32]);
void CRYPT_SHA256Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[32]);

void CRYPT_SHA384Start(CRYPT_sha384_context* context);
void CRYPT_SHA384Update(CRYPT_sha384_context* context,
                        const uint8_t* data,
                        uint32_t size);
void CRYPT_SHA384Finish(CRYPT_sha384_context* context, uint8_t digest[48]);
void CRYPT_SHA384Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[48]);

void CRYPT_SHA512Start(void* context);
void CRYPT_SHA512Update(void* context, const uint8_t* data, uint32_t size);
void CRYPT_SHA512Finish(void* context, uint8_t digest[64]);
void CRYPT_SHA512Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[64]);
#ifdef __cplusplus
};
#endif

#endif  // CORE_FDRM_CRYPTO_FX_CRYPT_H_
