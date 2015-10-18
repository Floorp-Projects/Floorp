/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "mozilla/Attributes.h"
#include "nsIScriptableBase64Encoder.h"
#include "nsIInputStream.h"
#include "nsAutoPtr.h"
#include "nsStringAPI.h"
#include <wchar.h>

struct Chunk {
  Chunk(uint32_t l, const char* c)
    : mLength(l), mData(c)
  {}

  uint32_t mLength;
  const char* mData;
};

struct Test {
  Test(Chunk* c, const char* r)
    : mChunks(c), mResult(r)
  {}

  Chunk* mChunks;
  const char* mResult;
};

static Chunk kTest1Chunks[] =
{
   Chunk(9, "Hello sir"),
   Chunk(0, nullptr)
};

static Chunk kTest2Chunks[] =
{
   Chunk(3, "Hel"),
   Chunk(3, "lo "),
   Chunk(3, "sir"),
   Chunk(0, nullptr)
};

static Chunk kTest3Chunks[] =
{
   Chunk(1, "I"),
   Chunk(0, nullptr)
};

static Chunk kTest4Chunks[] =
{
   Chunk(2, "Hi"),
   Chunk(0, nullptr)
};

static Chunk kTest5Chunks[] =
{
   Chunk(1, "B"),
   Chunk(2, "ob"),
   Chunk(0, nullptr)
};

static Chunk kTest6Chunks[] =
{
   Chunk(2, "Bo"),
   Chunk(1, "b"),
   Chunk(0, nullptr)
};

static Chunk kTest7Chunks[] =
{
   Chunk(1, "F"),    // Carry over 1
   Chunk(4, "iref"), // Carry over 2
   Chunk(2, "ox"),   //            1
   Chunk(4, " is "), //            2
   Chunk(2, "aw"),   //            1
   Chunk(4, "esom"), //            2
   Chunk(2, "e!"),
   Chunk(0, nullptr)
};

static Chunk kTest8Chunks[] =
{
   Chunk(5, "ALL T"),
   Chunk(1, "H"),
   Chunk(4, "ESE "),
   Chunk(2, "WO"),
   Chunk(21, "RLDS ARE YOURS EXCEPT"),
   Chunk(9, " EUROPA. "),
   Chunk(25, "ATTEMPT NO LANDING THERE."),
   Chunk(0, nullptr)
};

static Test kTests[] =
  {
    // Test 1, test a simple round string in one chunk
    Test(
      kTest1Chunks,
      "SGVsbG8gc2ly"
    ),
    // Test 2, test a simple round string split into round chunks
    Test(
      kTest2Chunks,
      "SGVsbG8gc2ly"
    ),
    // Test 3, test a single chunk that's 2 short
    Test(
      kTest3Chunks,
      "SQ=="
    ),
    // Test 4, test a single chunk that's 1 short
    Test(
      kTest4Chunks,
      "SGk="
    ),
    // Test 5, test a single chunk that's 2 short, followed by a chunk of 2
    Test(
      kTest5Chunks,
      "Qm9i"
    ),
    // Test 6, test a single chunk that's 1 short, followed by a chunk of 1
    Test(
      kTest6Chunks,
      "Qm9i"
    ),
    // Test 7, test alternating carryovers
    Test(
      kTest7Chunks,
      "RmlyZWZveCBpcyBhd2Vzb21lIQ=="
    ),
    // Test 8, test a longish string
    Test(
      kTest8Chunks,
      "QUxMIFRIRVNFIFdPUkxEUyBBUkUgWU9VUlMgRVhDRVBUIEVVUk9QQS4gQVRURU1QVCBOTyBMQU5ESU5HIFRIRVJFLg=="
    ),
    // Terminator
    Test(
      nullptr,
      nullptr
    )
  };

class FakeInputStream final : public nsIInputStream
{
  ~FakeInputStream() {}

public:

  FakeInputStream()
  : mTestNumber(0),
    mTest(&kTests[0]),
    mChunk(&mTest->mChunks[0]),
    mClosed(false)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

  void Reset();
  bool NextTest();
  bool CheckTest(nsACString& aResult);
  bool CheckTest(nsAString& aResult);
private:
  uint32_t mTestNumber;
  const Test* mTest;
  const Chunk* mChunk;
  bool mClosed;
};

NS_IMPL_ISUPPORTS(FakeInputStream, nsIInputStream)

NS_IMETHODIMP
FakeInputStream::Close()
{
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::Available(uint64_t* aAvailable)
{
  *aAvailable = 0;

  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  const Chunk* chunk = mChunk;
  while (chunk->mLength) {
    *aAvailable += chunk->mLength;
    chunk++;
  }

  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aOut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FakeInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                              void* aClosure,
                              uint32_t aCount,
                              uint32_t* aRead)
{
  *aRead = 0;

  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  while (mChunk->mLength) {
    uint32_t written = 0;

    nsresult rv = (*aWriter)(this, aClosure, mChunk->mData,
                             *aRead, mChunk->mLength, &written);

    *aRead += written;
    NS_ENSURE_SUCCESS(rv, rv);

    mChunk++;
  }

  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::IsNonBlocking(bool* aIsBlocking)
{
  *aIsBlocking = false;
  return NS_OK;
}

void
FakeInputStream::Reset()
{
  mClosed = false;
  mChunk = &mTest->mChunks[0];
}

bool
FakeInputStream::NextTest()
{
  mTestNumber++;
  mTest = &kTests[mTestNumber];
  mChunk = &mTest->mChunks[0];
  mClosed = false;

  return mTest->mChunks ? true : false;
}

bool
FakeInputStream::CheckTest(nsACString& aResult)
{
  return !strcmp(aResult.BeginReading(), mTest->mResult) ? true : false;
}

#ifdef XP_WIN
static inline int NS_tstrcmp(char16ptr_t x, char16ptr_t y) {
    return wcscmp(x, y);
}
#else
#define NS_tstrcmp strcmp
#endif

bool
FakeInputStream::CheckTest(nsAString& aResult)
{
  return !NS_tstrcmp(aResult.BeginReading(),
                     NS_ConvertASCIItoUTF16(mTest->mResult).BeginReading())
                     ? true : false;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("Base64");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  nsCOMPtr<nsIScriptableBase64Encoder> encoder =
    do_CreateInstance("@mozilla.org/scriptablebase64encoder;1");
  NS_ENSURE_TRUE(encoder, 1);

  RefPtr<FakeInputStream> stream = new FakeInputStream();
  do {
    nsString wideString;
    nsCString string;

    nsresult rv;
    rv = encoder->EncodeToString(stream, 0, wideString);
    NS_ENSURE_SUCCESS(rv, 1);

    stream->Reset();

    rv = encoder->EncodeToCString(stream, 0, string);
    NS_ENSURE_SUCCESS(rv, 1);

    if (!stream->CheckTest(wideString) || !stream->CheckTest(string))
      fail("Failed to convert properly\n");

  } while (stream->NextTest());

  return 0;
}
