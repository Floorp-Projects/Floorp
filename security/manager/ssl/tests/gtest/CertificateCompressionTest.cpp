/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"
#include "sslproto.h"
#include "sslerr.h"

#include "gtest/gtest.h"
#include "zlib.h"

class psm_CertificateCompression_Zlib : public ::testing::Test {};

/* A simple function to decode a zlib buffer. */
int zlib_encode(unsigned char* in, size_t inLen, unsigned char* out,
                size_t outLen, size_t* lengthEncoded) {
  int ret;
  z_stream strm;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit(&strm, 9);
  if (ret != Z_OK) return ret;

  strm.avail_in = inLen;
  strm.next_in = in;

  strm.avail_out = outLen;
  strm.next_out = out;
  ret = deflate(&strm, Z_FINISH);
  assert(strm.avail_in == 0);
  assert(ret == Z_STREAM_END);

  (void)deflateEnd(&strm);
  *lengthEncoded = strm.total_out;
  return Z_OK;
}

/* Legit encoded buffer*/
uint8_t encodedBufferExampleZlib[282] = {
    0x78, 0xda, 0x63, 0x60, 0x64, 0x62, 0x66, 0x61, 0x65, 0x63, 0xe7, 0xe0,
    0xe4, 0xe2, 0xe6, 0xe1, 0xe5, 0xe3, 0x17, 0x10, 0x14, 0x12, 0x16, 0x11,
    0x15, 0x13, 0x97, 0x90, 0x94, 0x92, 0x96, 0x91, 0x95, 0x93, 0x57, 0x50,
    0x54, 0x52, 0x56, 0x51, 0x55, 0x53, 0xd7, 0xd0, 0xd4, 0xd2, 0xd6, 0xd1,
    0xd5, 0xd3, 0x37, 0x30, 0x34, 0x32, 0x36, 0x31, 0x35, 0x33, 0xb7, 0xb0,
    0xb4, 0xb2, 0xb6, 0xb1, 0xb5, 0xb3, 0x77, 0x70, 0x74, 0x72, 0x76, 0x71,
    0x75, 0x73, 0xf7, 0xf0, 0xf4, 0xf2, 0xf6, 0xf1, 0xf5, 0xf3, 0xf,  0x8,
    0xc,  0xa,  0xe,  0x9,  0xd,  0xb,  0x8f, 0x88, 0x8c, 0x8a, 0x8e, 0x89,
    0x8d, 0x8b, 0x4f, 0x48, 0x4c, 0x4a, 0x4e, 0x49, 0x4d, 0x4b, 0xcf, 0xc8,
    0xcc, 0xca, 0xce, 0xc9, 0xcd, 0xcb, 0x2f, 0x28, 0x2c, 0x2a, 0x2e, 0x29,
    0x2d, 0x2b, 0xaf, 0xa8, 0xac, 0xaa, 0xae, 0xa9, 0xad, 0xab, 0x6f, 0x68,
    0x6c, 0x6a, 0x6e, 0x69, 0x6d, 0x6b, 0xef, 0xe8, 0xec, 0xea, 0xee, 0xe9,
    0xed, 0xeb, 0x9f, 0x30, 0x71, 0xd2, 0xe4, 0x29, 0x53, 0xa7, 0x4d, 0x9f,
    0x31, 0x73, 0xd6, 0xec, 0x39, 0x73, 0xe7, 0xcd, 0x5f, 0xb0, 0x70, 0xd1,
    0xe2, 0x25, 0x4b, 0x97, 0x2d, 0x5f, 0xb1, 0x72, 0xd5, 0xea, 0x35, 0x6b,
    0xd7, 0xad, 0xdf, 0xb0, 0x71, 0xd3, 0xe6, 0x2d, 0x5b, 0xb7, 0x6d, 0xdf,
    0xb1, 0x73, 0xd7, 0xee, 0x3d, 0x7b, 0xf7, 0xed, 0x3f, 0x70, 0xf0, 0xd0,
    0xe1, 0x23, 0x47, 0x8f, 0x1d, 0x3f, 0x71, 0xf2, 0xd4, 0xe9, 0x33, 0x67,
    0xcf, 0x9d, 0xbf, 0x70, 0xf1, 0xd2, 0xe5, 0x2b, 0x57, 0xaf, 0x5d, 0xbf,
    0x71, 0xf3, 0xd6, 0xed, 0x3b, 0x77, 0xef, 0xdd, 0x7f, 0xf0, 0xf0, 0xd1,
    0xe3, 0x27, 0x4f, 0x9f, 0x3d, 0x7f, 0xf1, 0xf2, 0xd5, 0xeb, 0x37, 0x6f,
    0xdf, 0xbd, 0xff, 0xf0, 0xf1, 0xd3, 0xe7, 0x2f, 0x5f, 0xbf, 0x7d, 0xff,
    0xf1, 0xf3, 0xd7, 0xef, 0x3f, 0x7f, 0xff, 0xfd, 0x67, 0x18, 0x81, 0xfe,
    0x7,  0x0,  0x2f, 0x9d, 0xf3, 0x4f};

/* This test checks that the encoding/decoding returns the same result. */
TEST_F(psm_CertificateCompression_Zlib, ZlibCorrectlyDecodesEncodedBuffer) {
  unsigned char in[500];
  for (size_t i = 0; i < sizeof(in); i++) {
    in[i] = i;
  }

  unsigned char encodedBuffer[500] = {0};
  size_t lengthEncoded = 0;

  int resultDef = zlib_encode(in, sizeof(in), encodedBuffer,
                              sizeof(encodedBuffer), &lengthEncoded);
  ASSERT_EQ(resultDef, Z_OK);

  SECItem encodedItem = {siBuffer, encodedBuffer, (unsigned int)lengthEncoded};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  SECStatus rv = zlibCertificateDecode(&encodedItem, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(usedLen, sizeof(in));
  ASSERT_EQ(0, PORT_Memcmp(decodedEncodedBuffer, in, sizeof(in)));
  ASSERT_EQ(
      0, PORT_Memcmp(encodedBufferExampleZlib, encodedBuffer, lengthEncoded));
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFailsEmptyInputItem) {
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* nullptr instead of input */
  SECStatus rv = zlibCertificateDecode(nullptr, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFailsEmptyInput) {
  SECItem encodedItem = {siBuffer, nullptr,
                         (unsigned int)sizeof(encodedBufferExampleZlib)};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* nullptr instead of input.data */
  SECStatus rv = zlibCertificateDecode(&encodedItem, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFails0LenInput) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleZlib, 0};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* 0 instead of input.len*/
  SECStatus rv = zlibCertificateDecode(&encodedItem, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFailsEmptyBuffer) {
  unsigned char encodedBuffer[500] = {0};
  SECItem encodedItem = {siBuffer, encodedBuffer, 500};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv = zlibCertificateDecode(&encodedItem, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFailsNullOutput) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleZlib,
                         (unsigned int)sizeof(encodedBufferExampleZlib)};
  uint8_t decodedEncodedBuffer[5] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv = zlibCertificateDecode(&encodedItem, nullptr,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Zlib, ZlibDecodingFailsOutputTooSmall) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleZlib,
                         (unsigned int)sizeof(encodedBufferExampleZlib)};
  uint8_t decodedEncodedBuffer[5] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv = zlibCertificateDecode(&encodedItem, decodedEncodedBuffer,
                                       sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

class psm_CertificateCompression_Brotli : public ::testing::Test {};

uint8_t encodedBufferExampleBrotli[60] = {
    0x8b, 0x1b, 0x80, 0x4c, 0x6f, 0x72, 0x65, 0x6d, 0x20, 0x69, 0x70, 0x73,
    0x75, 0x6d, 0x20, 0x64, 0x6f, 0x6c, 0x6f, 0x72, 0x20, 0x73, 0x69, 0x74,
    0x20, 0x61, 0x6d, 0x65, 0x74, 0x2c, 0x20, 0x63, 0x6f, 0x6e, 0x73, 0x65,
    0x63, 0x74, 0x65, 0x74, 0x75, 0x72, 0x20, 0x61, 0x64, 0x69, 0x70, 0x69,
    0x73, 0x63, 0x69, 0x6e, 0x67, 0x20, 0x65, 0x6c, 0x69, 0x74, 0x2e, 0x03};

TEST_F(psm_CertificateCompression_Brotli, BrotliCorrectlyDecodesEncodedBuffer) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleBrotli,
                         sizeof(encodedBufferExampleBrotli)};
  uint8_t decodedEncodedBuffer[100] = {0};
  size_t usedLen = sizeof(decodedEncodedBuffer);

  SECStatus rv =
      brotliCertificateDecode(&encodedItem, decodedEncodedBuffer,
                              sizeof(decodedEncodedBuffer), &usedLen);

  ASSERT_EQ(SECSuccess, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFailsEmptyInputItem) {
  uint8_t decodedEncodedBuffer[100] = {0};
  size_t usedLen = 0;
  /* nullptr instead of input */
  SECStatus rv = brotliCertificateDecode(
      nullptr, decodedEncodedBuffer, sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFailsEmptyInput) {
  SECItem encodedItem = {siBuffer, nullptr,
                         (unsigned int)sizeof(encodedBufferExampleBrotli)};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* nullptr instead of input.data */
  SECStatus rv =
      brotliCertificateDecode(&encodedItem, decodedEncodedBuffer,
                              sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFails0LenInput) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleBrotli, 0};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* 0 instead of input.len*/
  SECStatus rv =
      brotliCertificateDecode(&encodedItem, decodedEncodedBuffer,
                              sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFailsEmptyBuffer) {
  unsigned char encodedBuffer[500] = {0};
  SECItem encodedItem = {siBuffer, encodedBuffer, 500};
  uint8_t decodedEncodedBuffer[500] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv =
      brotliCertificateDecode(&encodedItem, decodedEncodedBuffer,
                              sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFailsNullOutput) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleBrotli,
                         (unsigned int)sizeof(encodedBufferExampleBrotli)};
  uint8_t decodedEncodedBuffer[5] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv = brotliCertificateDecode(
      &encodedItem, nullptr, sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(psm_CertificateCompression_Brotli, BrotliDecodingFailsOutputTooSmall) {
  SECItem encodedItem = {siBuffer, encodedBufferExampleBrotli,
                         (unsigned int)sizeof(encodedBufferExampleBrotli)};
  uint8_t decodedEncodedBuffer[5] = {0};
  size_t usedLen = 0;
  /* Empty buffer will return an error if decoded. */
  SECStatus rv =
      brotliCertificateDecode(&encodedItem, decodedEncodedBuffer,
                              sizeof(decodedEncodedBuffer), &usedLen);
  ASSERT_EQ(SECFailure, rv);
}
