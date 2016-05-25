/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTSerialization.h"
#include "CTTestUtils.h"

#include "gtest/gtest.h"

namespace mozilla { namespace ct {

using namespace pkix;

class CTSerializationTest : public ::testing::Test
{
public:
  void SetUp() override
  {
    mTestDigitallySigned = GetTestDigitallySigned();
    mTestSignatureData = GetTestDigitallySignedData();
  }

protected:
  Buffer mTestDigitallySigned;
  Buffer mTestSignatureData;
};

TEST_F(CTSerializationTest, DecodesDigitallySigned)
{
  Input digitallySigned = InputForBuffer(mTestDigitallySigned);
  Reader digitallySignedReader(digitallySigned);

  DigitallySigned parsed;
  ASSERT_EQ(Success,
    DecodeDigitallySigned(digitallySignedReader, parsed));
  EXPECT_TRUE(digitallySignedReader.AtEnd());

  EXPECT_EQ(DigitallySigned::HashAlgorithm::SHA256,
            parsed.hashAlgorithm);
  EXPECT_EQ(DigitallySigned::SignatureAlgorithm::ECDSA,
            parsed.signatureAlgorithm);
  EXPECT_EQ(mTestSignatureData, parsed.signatureData);
}

TEST_F(CTSerializationTest, FailsToDecodePartialDigitallySigned)
{
  Input partial;
  ASSERT_EQ(Success,
    partial.Init(mTestDigitallySigned.begin(),
      mTestDigitallySigned.length() - 5));
  Reader partialReader(partial);

  DigitallySigned parsed;

  EXPECT_NE(Success, DecodeDigitallySigned(partialReader, parsed));
}

TEST_F(CTSerializationTest, EncodesDigitallySigned)
{
  DigitallySigned digitallySigned;
  digitallySigned.hashAlgorithm =
    DigitallySigned::HashAlgorithm::SHA256;
  digitallySigned.signatureAlgorithm =
    DigitallySigned::SignatureAlgorithm::ECDSA;
  digitallySigned.signatureData = cloneBuffer(mTestSignatureData);

  Buffer encoded;

  ASSERT_EQ(Success, EncodeDigitallySigned(digitallySigned, encoded));
  EXPECT_EQ(mTestDigitallySigned, encoded);
}

TEST_F(CTSerializationTest, EncodesLogEntryForX509Cert)
{
  LogEntry entry;
  GetX509CertLogEntry(entry);

  Buffer encoded;
  ASSERT_EQ(Success, EncodeLogEntry(entry, encoded));
  EXPECT_EQ((718U + 5U), encoded.length());
  // First two bytes are log entry type. Next, length:
  // Length is 718 which is 512 + 206, which is 0x2ce
  Buffer expectedPrefix;
  MOZ_RELEASE_ASSERT(expectedPrefix.append("\0\0\0\x2\xCE", 5));
  Buffer encodedPrefix;
  MOZ_RELEASE_ASSERT(encodedPrefix.
    append(encoded.begin(), encoded.begin() + 5));
  EXPECT_EQ(expectedPrefix, encodedPrefix);
}

TEST_F(CTSerializationTest, EncodesLogEntryForPrecert)
{
  LogEntry entry;
  GetPrecertLogEntry(entry);

  Buffer encoded;
  ASSERT_EQ(Success, EncodeLogEntry(entry, encoded));
  // log entry type + issuer key + length + tbsCertificate
  EXPECT_EQ((2U + 32U + 3U + entry.tbsCertificate.length()), encoded.length());

  // First two bytes are log entry type.
  Buffer expectedPrefix;
  MOZ_RELEASE_ASSERT(expectedPrefix.append("\0\x1", 2));
  Buffer encodedPrefix;
  MOZ_RELEASE_ASSERT(encodedPrefix.
    append(encoded.begin(), encoded.begin() + 2));
  EXPECT_EQ(expectedPrefix, encodedPrefix);

  // Next is the issuer key (32 bytes).
  Buffer encodedKeyHash;
  MOZ_RELEASE_ASSERT(encodedKeyHash.
    append(encoded.begin() + 2, encoded.begin() + 2 + 32));
  EXPECT_EQ(GetDefaultIssuerKeyHash(), encodedKeyHash);
}

TEST_F(CTSerializationTest, EncodesV1SCTSignedData)
{
  uint64_t timestamp = UINT64_C(0x139fe353cf5);
  const uint8_t DUMMY_BYTES[] = { 0x61, 0x62, 0x63 }; // abc
  Input dummyEntry(DUMMY_BYTES);
  Input emptyExtensions;
  Buffer encoded;
  ASSERT_EQ(Success, EncodeV1SCTSignedData(
    timestamp, dummyEntry, emptyExtensions, encoded));
  EXPECT_EQ((size_t) 15, encoded.length());

  const uint8_t EXPECTED_BYTES[] = {
    0x00, // version
    0x00, // signature type
    0x00, 0x00, 0x01, 0x39, 0xFE, 0x35, 0x3C, 0xF5, // timestamp
    0x61, 0x62, 0x63, // log signature
    0x00, 0x00 // extensions (empty)
  };
  Buffer expectedBuffer;
  MOZ_RELEASE_ASSERT(
    expectedBuffer.append(EXPECTED_BYTES, sizeof(EXPECTED_BYTES)));
  EXPECT_EQ(expectedBuffer, encoded);
}

TEST_F(CTSerializationTest, DecodesSCTList)
{
  // Two items in the list: "abc", "def"
  const uint8_t ENCODED[] = {
    0x00, 0x0a, 0x00, 0x03, 0x61, 0x62, 0x63, 0x00, 0x03, 0x64, 0x65, 0x66
  };
  const uint8_t DECODED_1[] = { 0x61, 0x62, 0x63 };
  const uint8_t DECODED_2[] = { 0x64, 0x65, 0x66 };

  Reader listReader;
  ASSERT_EQ(Success, DecodeSCTList(Input(ENCODED), listReader));

  Input decoded1;
  ASSERT_EQ(Success, ReadSCTListItem(listReader, decoded1));

  Input decoded2;
  ASSERT_EQ(Success, ReadSCTListItem(listReader, decoded2));

  EXPECT_TRUE(listReader.AtEnd());
  EXPECT_TRUE(InputsAreEqual(decoded1, Input(DECODED_1)));
  EXPECT_TRUE(InputsAreEqual(decoded2, Input(DECODED_2)));
}

TEST_F(CTSerializationTest, FailsDecodingInvalidSCTList)
{
  // A list with one item that's too short (the second one)
  const uint8_t ENCODED[] = {
    0x00, 0x0a, 0x00, 0x03, 0x61, 0x62, 0x63, 0x00, 0x05, 0x64, 0x65, 0x66
  };

  Reader listReader;
  ASSERT_EQ(Success, DecodeSCTList(Input(ENCODED), listReader));
  Input decoded1;
  EXPECT_EQ(Success, ReadSCTListItem(listReader, decoded1));
  Input decoded2;
  EXPECT_NE(Success, ReadSCTListItem(listReader, decoded2));
}

TEST_F(CTSerializationTest, DecodesSignedCertificateTimestamp)
{
  Buffer encodedSctBuffer = GetTestSignedCertificateTimestamp();
  Input encodedSctInput = InputForBuffer(encodedSctBuffer);
  Reader encodedSctReader(encodedSctInput);

  SignedCertificateTimestamp sct;
  ASSERT_EQ(Success,
    DecodeSignedCertificateTimestamp(encodedSctReader, sct));
  EXPECT_EQ(SignedCertificateTimestamp::Version::V1, sct.version);
  EXPECT_EQ(GetTestPublicKeyId(), sct.logId);
  const uint64_t expectedTime = 1365181456089;
  EXPECT_EQ(expectedTime, sct.timestamp);
  const size_t expectedSignatureLength = 71;
  EXPECT_EQ(expectedSignatureLength, sct.signature.signatureData.length());
  EXPECT_TRUE(sct.extensions.empty());
}

TEST_F(CTSerializationTest, FailsDecodingInvalidSignedCertificateTimestamp)
{
  SignedCertificateTimestamp sct;

  // Invalid version
  const uint8_t INVALID_VERSION_BYTES[] = { 0x02, 0x00 };
  Input invalidVersionSctInput(INVALID_VERSION_BYTES);
  Reader invalidVersionSctReader(invalidVersionSctInput);
  EXPECT_EQ(Result::ERROR_BAD_DER,
    DecodeSignedCertificateTimestamp(invalidVersionSctReader, sct));

  // Valid version, invalid length (missing data)
  const uint8_t INVALID_LENGTH_BYTES[] = { 0x00, 0x0a, 0x0b, 0x0c };
  Input invalidLengthSctInput(INVALID_LENGTH_BYTES);
  Reader invalidLengthSctReader(invalidLengthSctInput);
  EXPECT_EQ(Result::ERROR_BAD_DER,
    DecodeSignedCertificateTimestamp(invalidLengthSctReader, sct));
}

TEST_F(CTSerializationTest, EncodesValidSignedTreeHead)
{
  SignedTreeHead signedTreeHead;
  GetSampleSignedTreeHead(signedTreeHead);

  Buffer encoded;
  ASSERT_EQ(Success,
    EncodeTreeHeadSignature(signedTreeHead, encoded));
  // Expected size is 50 bytes:
  // Byte 0 is version, byte 1 is signature type
  // Bytes 2-9 are timestamp
  // Bytes 10-17 are tree size
  // Bytes 18-49 are sha256 root hash
  ASSERT_EQ(50u, encoded.length());
  const uint8_t EXPECTED_BYTES_PREFIX[] = {
    0x00, // version
    0x01, // signature type
    0x00, 0x00, 0x01, 0x45, 0x3c, 0x5f, 0xb8, 0x35, // timestamp
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15  // tree size
    // sha256 root hash should follow
  };
  Buffer expectedBuffer;
  MOZ_RELEASE_ASSERT(expectedBuffer.append(EXPECTED_BYTES_PREFIX, 18));
  Buffer hash = GetSampleSTHSHA256RootHash();
  MOZ_RELEASE_ASSERT(expectedBuffer.append(hash.begin(), hash.length()));
  EXPECT_EQ(expectedBuffer, encoded);
}

} }  // namespace mozilla::ct
