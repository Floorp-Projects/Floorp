/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "gtest/gtest.h"
#include "Helpers.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "nsIPipe.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsTArray.h"

namespace {

using mozilla::SnappyCompressOutputStream;
using mozilla::SnappyUncompressInputStream;

static already_AddRefed<nsIOutputStream> CompressPipe(
    nsIInputStream** aReaderOut) {
  nsCOMPtr<nsIOutputStream> pipeWriter;

  nsresult rv = NS_NewPipe(aReaderOut, getter_AddRefs(pipeWriter));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsCOMPtr<nsIOutputStream> compress =
      new SnappyCompressOutputStream(pipeWriter);
  return compress.forget();
}

// Verify the given number of bytes compresses to a smaller number of bytes.
static void TestCompress(uint32_t aNumBytes) {
  // Don't permit this test on small data sizes as snappy can slightly
  // bloat very small content.
  ASSERT_GT(aNumBytes, 1024u);

  nsCOMPtr<nsIInputStream> pipeReader;
  nsCOMPtr<nsIOutputStream> compress = CompressPipe(getter_AddRefs(pipeReader));
  ASSERT_TRUE(compress);

  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);

  testing::WriteAllAndClose(compress, inputData);

  nsAutoCString outputData;
  nsresult rv = NS_ConsumeStream(pipeReader, UINT32_MAX, outputData);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_LT(outputData.Length(), inputData.Length());
}

// Verify that the given number of bytes can be compressed and uncompressed
// successfully.
static void TestCompressUncompress(uint32_t aNumBytes) {
  nsCOMPtr<nsIInputStream> pipeReader;
  nsCOMPtr<nsIOutputStream> compress = CompressPipe(getter_AddRefs(pipeReader));
  ASSERT_TRUE(compress);

  nsCOMPtr<nsIInputStream> uncompress =
      new SnappyUncompressInputStream(pipeReader);

  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);

  testing::WriteAllAndClose(compress, inputData);

  nsAutoCString outputData;
  nsresult rv = NS_ConsumeStream(uncompress, UINT32_MAX, outputData);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_EQ(inputData.Length(), outputData.Length());
  for (uint32_t i = 0; i < inputData.Length(); ++i) {
    EXPECT_EQ(inputData[i], outputData.get()[i]) << "Byte " << i;
  }
}

static void TestUncompressCorrupt(const char* aCorruptData,
                                  uint32_t aCorruptLength) {
  nsCOMPtr<nsIInputStream> source;
  nsresult rv = NS_NewByteInputStream(
      getter_AddRefs(source), mozilla::MakeSpan(aCorruptData, aCorruptLength),
      NS_ASSIGNMENT_DEPEND);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> uncompress = new SnappyUncompressInputStream(source);

  nsAutoCString outputData;
  rv = NS_ConsumeStream(uncompress, UINT32_MAX, outputData);
  ASSERT_EQ(NS_ERROR_CORRUPTED_CONTENT, rv);
}

}  // namespace

TEST(SnappyStream, Compress_32k)
{ TestCompress(32 * 1024); }

TEST(SnappyStream, Compress_64k)
{ TestCompress(64 * 1024); }

TEST(SnappyStream, Compress_128k)
{ TestCompress(128 * 1024); }

TEST(SnappyStream, CompressUncompress_0)
{ TestCompressUncompress(0); }

TEST(SnappyStream, CompressUncompress_1)
{ TestCompressUncompress(1); }

TEST(SnappyStream, CompressUncompress_32)
{ TestCompressUncompress(32); }

TEST(SnappyStream, CompressUncompress_1k)
{ TestCompressUncompress(1024); }

TEST(SnappyStream, CompressUncompress_32k)
{ TestCompressUncompress(32 * 1024); }

TEST(SnappyStream, CompressUncompress_64k)
{ TestCompressUncompress(64 * 1024); }

TEST(SnappyStream, CompressUncompress_128k)
{ TestCompressUncompress(128 * 1024); }

// Test buffers that are not exactly power-of-2 in length to try to
// exercise more edge cases.  The number 13 is arbitrary.

TEST(SnappyStream, CompressUncompress_256k_less_13)
{ TestCompressUncompress((256 * 1024) - 13); }

TEST(SnappyStream, CompressUncompress_256k)
{ TestCompressUncompress(256 * 1024); }

TEST(SnappyStream, CompressUncompress_256k_plus_13)
{ TestCompressUncompress((256 * 1024) + 13); }

TEST(SnappyStream, UncompressCorruptStreamIdentifier)
{
  static const char data[] = "This is not a valid compressed stream";
  TestUncompressCorrupt(data, strlen(data));
}

TEST(SnappyStream, UncompressCorruptCompressedDataLength)
{
  static const char data[] =
      "\xff\x06\x00\x00sNaPpY"  // stream identifier
      "\x00\x99\x00\x00This is not a valid compressed stream";
  static const uint32_t dataLength = (sizeof(data) / sizeof(const char)) - 1;
  TestUncompressCorrupt(data, dataLength);
}

TEST(SnappyStream, UncompressCorruptCompressedDataContent)
{
  static const char data[] =
      "\xff\x06\x00\x00sNaPpY"  // stream identifier
      "\x00\x25\x00\x00This is not a valid compressed stream";
  static const uint32_t dataLength = (sizeof(data) / sizeof(const char)) - 1;
  TestUncompressCorrupt(data, dataLength);
}
