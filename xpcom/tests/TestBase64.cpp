/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Base 64 Encoder Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Huey <me@kylehuey.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestHarness.h"

#include "nsIScriptableBase64Encoder.h"
#include "nsIInputStream.h"
#include "nsAutoPtr.h"
#include "nsStringAPI.h"
#include <wchar.h>

struct Chunk {
  Chunk(PRUint32 l, const char* c)
    : mLength(l), mData(c)
  {}

  PRUint32 mLength;
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
   Chunk(0, nsnull)
};

static Chunk kTest2Chunks[] =
{
   Chunk(3, "Hel"),
   Chunk(3, "lo "),
   Chunk(3, "sir"),
   Chunk(0, nsnull)
};

static Chunk kTest3Chunks[] =
{
   Chunk(1, "I"),
   Chunk(0, nsnull)
};

static Chunk kTest4Chunks[] =
{
   Chunk(2, "Hi"),
   Chunk(0, nsnull)
};

static Chunk kTest5Chunks[] =
{
   Chunk(1, "B"),
   Chunk(2, "ob"),
   Chunk(0, nsnull)
};

static Chunk kTest6Chunks[] =
{
   Chunk(2, "Bo"),
   Chunk(1, "b"),
   Chunk(0, nsnull)
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
   Chunk(0, nsnull)
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
   Chunk(0, nsnull)
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
      nsnull,
      nsnull
    )
  };

class FakeInputStream : public nsIInputStream
{
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
  PRUint32 mTestNumber;
  const Test* mTest;
  const Chunk* mChunk;
  bool mClosed;
};

NS_IMPL_ISUPPORTS1(FakeInputStream, nsIInputStream)

NS_IMETHODIMP
FakeInputStream::Close()
{
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
FakeInputStream::Available(PRUint32* aAvailable)
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
FakeInputStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* aOut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FakeInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                              void* aClosure,
                              PRUint32 aCount,
                              PRUint32* aRead)
{
  *aRead = 0;

  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  while (mChunk->mLength) {
    PRUint32 written = 0;

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
  *aIsBlocking = PR_FALSE;
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
#define NS_tstrcmp wcscmp
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

  nsRefPtr<FakeInputStream> stream = new FakeInputStream();
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
