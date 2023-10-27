/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/Base64.h"
#include "nsComponentManagerUtils.h"
#include "nsIScriptableBase64Encoder.h"
#include "nsIInputStream.h"
#include "nsString.h"

#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"

struct Chunk {
  Chunk(uint32_t l, const char* c) : mLength(l), mData(c) {}

  uint32_t mLength;
  const char* mData;
};

struct Test {
  Test(Chunk* c, const char* r) : mChunks(c), mResult(r) {}

  Chunk* mChunks;
  const char* mResult;
};

static Chunk kTest1Chunks[] = {Chunk(9, "Hello sir"), Chunk(0, nullptr)};

static Chunk kTest2Chunks[] = {Chunk(3, "Hel"), Chunk(3, "lo "),
                               Chunk(3, "sir"), Chunk(0, nullptr)};

static Chunk kTest3Chunks[] = {Chunk(1, "I"), Chunk(0, nullptr)};

static Chunk kTest4Chunks[] = {Chunk(2, "Hi"), Chunk(0, nullptr)};

static Chunk kTest5Chunks[] = {Chunk(1, "B"), Chunk(2, "ob"),
                               Chunk(0, nullptr)};

static Chunk kTest6Chunks[] = {Chunk(2, "Bo"), Chunk(1, "b"),
                               Chunk(0, nullptr)};

static Chunk kTest7Chunks[] = {Chunk(1, "F"),     // Carry over 1
                               Chunk(4, "iref"),  // Carry over 2
                               Chunk(2, "ox"),    //            1
                               Chunk(4, " is "),  //            2
                               Chunk(2, "aw"),    //            1
                               Chunk(4, "esom"),  //            2
                               Chunk(2, "e!"),   Chunk(0, nullptr)};

static Chunk kTest8Chunks[] = {Chunk(5, "ALL T"),
                               Chunk(1, "H"),
                               Chunk(4, "ESE "),
                               Chunk(2, "WO"),
                               Chunk(21, "RLDS ARE YOURS EXCEPT"),
                               Chunk(9, " EUROPA. "),
                               Chunk(25, "ATTEMPT NO LANDING THERE."),
                               Chunk(0, nullptr)};

static Test kTests[] = {
    // Test 1, test a simple round string in one chunk
    Test(kTest1Chunks, "SGVsbG8gc2ly"),
    // Test 2, test a simple round string split into round chunks
    Test(kTest2Chunks, "SGVsbG8gc2ly"),
    // Test 3, test a single chunk that's 2 short
    Test(kTest3Chunks, "SQ=="),
    // Test 4, test a single chunk that's 1 short
    Test(kTest4Chunks, "SGk="),
    // Test 5, test a single chunk that's 2 short, followed by a chunk of 2
    Test(kTest5Chunks, "Qm9i"),
    // Test 6, test a single chunk that's 1 short, followed by a chunk of 1
    Test(kTest6Chunks, "Qm9i"),
    // Test 7, test alternating carryovers
    Test(kTest7Chunks, "RmlyZWZveCBpcyBhd2Vzb21lIQ=="),
    // Test 8, test a longish string
    Test(kTest8Chunks,
         "QUxMIFRIRVNFIFdPUkxEUyBBUkUgWU9VUlMgRVhDRVBUIEVVUk9QQS4gQVRURU1QVCBOT"
         "yBMQU5ESU5HIFRIRVJFLg=="),
    // Terminator
    Test(nullptr, nullptr)};

class FakeInputStream final : public nsIInputStream {
  ~FakeInputStream() = default;

 public:
  FakeInputStream()
      : mTestNumber(0),
        mTest(&kTests[0]),
        mChunk(&mTest->mChunks[0]),
        mClosed(false) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

  void Reset();
  bool NextTest();
  void CheckTest(nsACString& aResult);
  void CheckTest(nsAString& aResult);

 private:
  uint32_t mTestNumber;
  const Test* mTest;
  const Chunk* mChunk;
  bool mClosed;
};

NS_IMPL_ISUPPORTS(FakeInputStream, nsIInputStream)

NS_IMETHODIMP
FakeInputStream::Close() {
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::Available(uint64_t* aAvailable) {
  *aAvailable = 0;

  if (mClosed) return NS_BASE_STREAM_CLOSED;

  const Chunk* chunk = mChunk;
  while (chunk->mLength) {
    *aAvailable += chunk->mLength;
    chunk++;
  }

  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::StreamStatus() {
  return mClosed ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
FakeInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aOut) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FakeInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                              uint32_t aCount, uint32_t* aRead) {
  *aRead = 0;

  if (mClosed) return NS_BASE_STREAM_CLOSED;

  while (mChunk->mLength) {
    uint32_t written = 0;

    nsresult rv = (*aWriter)(this, aClosure, mChunk->mData, *aRead,
                             mChunk->mLength, &written);

    *aRead += written;
    NS_ENSURE_SUCCESS(rv, rv);

    mChunk++;
  }

  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::IsNonBlocking(bool* aIsBlocking) {
  *aIsBlocking = false;
  return NS_OK;
}

void FakeInputStream::Reset() {
  mClosed = false;
  mChunk = &mTest->mChunks[0];
}

bool FakeInputStream::NextTest() {
  mTestNumber++;
  mTest = &kTests[mTestNumber];
  mChunk = &mTest->mChunks[0];
  mClosed = false;

  return mTest->mChunks != nullptr;
}

void FakeInputStream::CheckTest(nsACString& aResult) {
  ASSERT_STREQ(aResult.BeginReading(), mTest->mResult);
}

void FakeInputStream::CheckTest(nsAString& aResult) {
  ASSERT_TRUE(aResult.EqualsASCII(mTest->mResult))
  << "Actual:   " << NS_ConvertUTF16toUTF8(aResult).get() << '\n'
  << "Expected: " << mTest->mResult;
}

TEST(Base64, StreamEncoder)
{
  nsCOMPtr<nsIScriptableBase64Encoder> encoder =
      do_CreateInstance("@mozilla.org/scriptablebase64encoder;1");
  ASSERT_TRUE(encoder);

  RefPtr<FakeInputStream> stream = new FakeInputStream();
  do {
    nsString wideString;
    nsCString string;

    nsresult rv;
    rv = encoder->EncodeToString(stream, 0, wideString);
    ASSERT_NS_SUCCEEDED(rv);

    stream->Reset();

    rv = encoder->EncodeToCString(stream, 0, string);
    ASSERT_NS_SUCCEEDED(rv);

    stream->CheckTest(wideString);
    stream->CheckTest(string);
  } while (stream->NextTest());
}

struct EncodeDecodeTestCase {
  const char* mInput;
  const char* mOutput;
};

static EncodeDecodeTestCase sRFC4648TestCases[] = {
    {"", ""},
    {"f", "Zg=="},
    {"fo", "Zm8="},
    {"foo", "Zm9v"},
    {"foob", "Zm9vYg=="},
    {"fooba", "Zm9vYmE="},
    {"foobar", "Zm9vYmFy"},
};

TEST(Base64, RFC4648Encoding)
{
  for (auto& testcase : sRFC4648TestCases) {
    nsDependentCString in(testcase.mInput);
    nsAutoCString out;
    nsresult rv = mozilla::Base64Encode(in, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsASCII(testcase.mOutput));
  }

  for (auto& testcase : sRFC4648TestCases) {
    NS_ConvertUTF8toUTF16 in(testcase.mInput);
    nsAutoString out;
    nsresult rv = mozilla::Base64Encode(in, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsASCII(testcase.mOutput));
  }
}

TEST(Base64, RFC4648Encoding_TransformAndAppend_EmptyPrefix)
{
  for (auto& testcase : sRFC4648TestCases) {
    nsDependentCString in(testcase.mInput);
    nsAutoString out;
    nsresult rv =
        mozilla::Base64EncodeAppend(in.BeginReading(), in.Length(), out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsASCII(testcase.mOutput));
  }
}

TEST(Base64, RFC4648Encoding_TransformAndAppend_NonEmptyPrefix)
{
  for (auto& testcase : sRFC4648TestCases) {
    nsDependentCString in(testcase.mInput);
    nsAutoString out{u"foo"_ns};
    nsresult rv =
        mozilla::Base64EncodeAppend(in.BeginReading(), in.Length(), out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(StringBeginsWith(out, u"foo"_ns));
    ASSERT_TRUE(Substring(out, 3).EqualsASCII(testcase.mOutput));
  }
}

TEST(Base64, RFC4648Decoding)
{
  for (auto& testcase : sRFC4648TestCases) {
    nsDependentCString out(testcase.mOutput);
    nsAutoCString in;
    nsresult rv = mozilla::Base64Decode(out, in);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(in.EqualsASCII(testcase.mInput));
  }

  for (auto& testcase : sRFC4648TestCases) {
    NS_ConvertUTF8toUTF16 out(testcase.mOutput);
    nsAutoString in;
    nsresult rv = mozilla::Base64Decode(out, in);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(in.EqualsASCII(testcase.mInput));
  }
}

TEST(Base64, RFC4648DecodingRawPointers)
{
  for (auto& testcase : sRFC4648TestCases) {
    size_t outputLength = strlen(testcase.mOutput);
    size_t inputLength = strlen(testcase.mInput);

    // This will be allocated by Base64Decode.
    char* buffer = nullptr;

    uint32_t binaryLength = 0;
    nsresult rv = mozilla::Base64Decode(testcase.mOutput, outputLength, &buffer,
                                        &binaryLength);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_EQ(binaryLength, inputLength);
    ASSERT_STREQ(testcase.mInput, buffer);
    free(buffer);
  }
}

static EncodeDecodeTestCase sNonASCIITestCases[] = {
    {"\x80", "gA=="},
    {"\xff", "/w=="},
    {"\x80\x80", "gIA="},
    {"\x80\x81", "gIE="},
    {"\xff\xff", "//8="},
    {"\x80\x80\x80", "gICA"},
    {"\xff\xff\xff", "////"},
    {"\x80\x80\x80\x80", "gICAgA=="},
    {"\xff\xff\xff\xff", "/////w=="},
};

TEST(Base64, NonASCIIEncoding)
{
  for (auto& testcase : sNonASCIITestCases) {
    nsDependentCString in(testcase.mInput);
    nsAutoCString out;
    nsresult rv = mozilla::Base64Encode(in, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsASCII(testcase.mOutput));
  }
}

TEST(Base64, NonASCIIEncodingWideString)
{
  for (auto& testcase : sNonASCIITestCases) {
    nsAutoString in, out;
    // XXX Handles Latin1 despite the name
    AppendASCIItoUTF16(nsDependentCString(testcase.mInput), in);
    nsresult rv = mozilla::Base64Encode(in, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsASCII(testcase.mOutput));
  }
}

TEST(Base64, NonASCIIDecoding)
{
  for (auto& testcase : sNonASCIITestCases) {
    nsDependentCString out(testcase.mOutput);
    nsAutoCString in;
    nsresult rv = mozilla::Base64Decode(out, in);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(in.Equals(testcase.mInput));
  }
}

TEST(Base64, NonASCIIDecodingWideString)
{
  for (auto& testcase : sNonASCIITestCases) {
    nsAutoString in, out;
    // XXX Handles Latin1 despite the name
    AppendASCIItoUTF16(nsDependentCString(testcase.mOutput), out);
    nsresult rv = mozilla::Base64Decode(out, in);
    ASSERT_NS_SUCCEEDED(rv);
    // Can't use EqualsASCII, because our comparison string isn't ASCII.
    for (size_t i = 0; i < in.Length(); ++i) {
      ASSERT_TRUE(((unsigned int)in[i] & 0xff00) == 0);
      ASSERT_EQ((unsigned char)in[i], (unsigned char)testcase.mInput[i]);
    }
    ASSERT_TRUE(strlen(testcase.mInput) == in.Length());
  }
}

// For historical reasons, our wide string base64 encode routines mask off
// the high bits of non-latin1 wide strings.
TEST(Base64, EncodeNon8BitWideString)
{
  {
    const nsAutoString non8Bit(u"\x1ff");
    nsAutoString out;
    nsresult rv = mozilla::Base64Encode(non8Bit, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsLiteral("/w=="));
  }
  {
    const nsAutoString non8Bit(u"\xfff");
    nsAutoString out;
    nsresult rv = mozilla::Base64Encode(non8Bit, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.EqualsLiteral("/w=="));
  }
}

// For historical reasons, our wide string base64 decode routines mask off
// the high bits of non-latin1 wide strings.
TEST(Base64, DecodeNon8BitWideString)
{
  {
    // This would be "/w==" in a nsCString
    const nsAutoString non8Bit(u"\x12f\x177==");
    const nsAutoString expectedOutput(u"\xff");
    ASSERT_EQ(non8Bit.Length(), 4u);
    nsAutoString out;
    nsresult rv = mozilla::Base64Decode(non8Bit, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.Equals(expectedOutput));
  }
  {
    const nsAutoString non8Bit(u"\xf2f\xf77==");
    const nsAutoString expectedOutput(u"\xff");
    nsAutoString out;
    nsresult rv = mozilla::Base64Decode(non8Bit, out);
    ASSERT_NS_SUCCEEDED(rv);
    ASSERT_TRUE(out.Equals(expectedOutput));
  }
}

TEST(Base64, DecodeWideTo8Bit)
{
  for (auto& testCase : sRFC4648TestCases) {
    const nsAutoCString in8bit(testCase.mOutput);
    const NS_ConvertUTF8toUTF16 inWide(testCase.mOutput);
    nsAutoCString out2;
    nsAutoCString out1;
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(mozilla::Base64Decode(inWide, out1)));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(mozilla::Base64Decode(in8bit, out2)));
    ASSERT_EQ(out1, out2);
  }
}

TEST(Base64, TruncateOnInvalidDecodeCString)
{
  constexpr auto invalid = "@@=="_ns;
  nsAutoCString out("I should be truncated!");
  nsresult rv = mozilla::Base64Decode(invalid, out);
  ASSERT_NS_FAILED(rv);
  ASSERT_EQ(out.Length(), 0u);
}

TEST(Base64, TruncateOnInvalidDecodeWideString)
{
  constexpr auto invalid = u"@@=="_ns;
  nsAutoString out(u"I should be truncated!");
  nsresult rv = mozilla::Base64Decode(invalid, out);
  ASSERT_NS_FAILED(rv);
  ASSERT_EQ(out.Length(), 0u);
}

// TODO: Add tests for OOM handling.
