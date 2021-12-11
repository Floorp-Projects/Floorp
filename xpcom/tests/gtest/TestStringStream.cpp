/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "Helpers.h"
#include "nsICloneableInputStream.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsStreamUtils.h"
#include "mozilla/Span.h"
#include "nsISeekableStream.h"

namespace {

static void TestStringStream(uint32_t aNumBytes) {
  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  testing::ConsumeAndValidateStream(stream, inputString);
}

static void TestStringStreamClone(uint32_t aNumBytes) {
  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(stream);
  ASSERT_TRUE(cloneable != nullptr);
  ASSERT_TRUE(cloneable->GetCloneable());

  nsCOMPtr<nsIInputStream> clone;
  rv = cloneable->Clone(getter_AddRefs(clone));

  testing::ConsumeAndValidateStream(stream, inputString);

  // Release the stream to verify that the clone's string survives correctly.
  stream = nullptr;

  testing::ConsumeAndValidateStream(clone, inputString);
}

}  // namespace

TEST(StringStream, Simple_4k)
{ TestStringStream(1024 * 4); }

TEST(StringStream, Clone_4k)
{ TestStringStreamClone(1024 * 4); }

static nsresult CloseStreamThenRead(nsIInputStream* aInStr, void* aClosure,
                                    const char* aBuffer, uint32_t aOffset,
                                    uint32_t aCount, uint32_t* aCountWritten) {
  // Closing the stream will free the data
  nsresult rv = aInStr->Close();
  if (NS_FAILED(rv)) {
    return rv;
  }
  // This will likely be allocated in the same slot as what we have in aBuffer
  char* newAlloc = moz_xstrdup("abcd");

  char* toBuf = static_cast<char*>(aClosure);
  memcpy(&toBuf[aOffset], aBuffer, aCount);
  *aCountWritten = aCount;
  free(newAlloc);
  return NS_OK;
}

TEST(StringStream, CancelInReadSegments)
{
  char* buffer = moz_xstrdup("test");
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(
      getter_AddRefs(stream), mozilla::Span(buffer, 5), NS_ASSIGNMENT_ADOPT);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  char buf[100];
  uint32_t count = 0;
  uint64_t available = 0;
  rv = stream->Available(&available);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->ReadSegments(CloseStreamThenRead, buf, available, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(count == 5);
  ASSERT_TRUE(!strcmp(buf, "test"));
}

// In bug 1595886 we added a diagnostic assert that the buffer of the
// string stream is not replaced by the closure passed to ReadSegments
// but if it is we restore it afterwards.
// The following test runs only if !MOZ_DIAGNOSTIC_ASSERT_ENABLED
// We check that the string is still what it was originally.
#ifndef MOZ_DIAGNOSTIC_ASSERT_ENABLED

static nsresult ChangeStreamThenRead(nsIInputStream* aInStr, void* aClosure,
                                     const char* aBuffer, uint32_t aOffset,
                                     uint32_t aCount, uint32_t* aCountWritten) {
  nsCOMPtr<nsIStringInputStream> stream = do_QueryInterface(aInStr);

  // This does free the old data, which may be safely stowed in a temp variable
  // We now do something that "no sane code in our codebase would do" ©
  // We set a new buffer. The pointer we got in aBuffer should still be valid
  // upon exiting ReadSegments and we are intentionally restoring it after
  // the call is over.
  char* newAlloc = moz_xstrdup("abcd");
  nsresult rv = stream->AdoptData(newAlloc, 5);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char* toBuf = static_cast<char*>(aClosure);
  memcpy(&toBuf[aOffset], aBuffer, aCount);
  *aCountWritten = aCount;
  return NS_OK;
}

TEST(StringStream, ReplacedInReadSegments)
{
  char* buffer = moz_xstrdup("test");
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(
      getter_AddRefs(stream), mozilla::Span(buffer, 5), NS_ASSIGNMENT_ADOPT);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  char buf[100];
  uint32_t count = 0;
  uint64_t available = 0;
  rv = stream->Available(&available);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->ReadSegments(ChangeStreamThenRead, buf, available, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(count == 5);
  ASSERT_TRUE(!strcmp(buf, "test"));
  buf[0] = '\0';
  count = 0;
  available = 0;
  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(stream);
  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->Available(&available);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->ReadSegments(NS_CopySegmentToBuffer, buf, available, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(count == 5);

  // Check that the StringStream's mData was restored after ChangeStreamThenRead
  // was executed.
  ASSERT_TRUE(!strcmp(buf, "test"));
}

#endif  // ifndef MOZ_DIAGNOSTIC_ASSERT_ENABLED
