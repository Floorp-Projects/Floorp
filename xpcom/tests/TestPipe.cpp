/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIPipe.h"
#include "nsIMemory.h"
#include "mozilla/Attributes.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

/** NS_NewPipe2 reimplemented, because it's not exported by XPCOM */
nsresult TP_NewPipe2(nsIAsyncInputStream** input,
                     nsIAsyncOutputStream** output,
                     bool nonBlockingInput,
                     bool nonBlockingOutput,
                     uint32_t segmentSize,
                     uint32_t segmentCount)
{
  nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
  if (!pipe)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = pipe->Init(nonBlockingInput,
                           nonBlockingOutput,
                           segmentSize,
                           segmentCount);

  if (NS_FAILED(rv))
    return rv;

  pipe->GetInputStream(input);
  pipe->GetOutputStream(output);
  return NS_OK;
}

nsresult TestPipe()
{
  const uint32_t SEGMENT_COUNT = 10;
  const uint32_t SEGMENT_SIZE = 10;

  nsCOMPtr<nsIAsyncInputStream> input;
  nsCOMPtr<nsIAsyncOutputStream> output;
  nsresult rv = TP_NewPipe2(getter_AddRefs(input),
                   getter_AddRefs(output),
                   false,
                   false,
                   SEGMENT_SIZE, SEGMENT_COUNT); 
  if (NS_FAILED(rv))
  {
    fail("TP_NewPipe2 failed: %x", rv);
    return rv;
  }

  const uint32_t BUFFER_LENGTH = 100;
  const char written[] =
    "0123456789"
    "1123456789"
    "2123456789"
    "3123456789"
    "4123456789"
    "5123456789"
    "6123456789"
    "7123456789"
    "8123456789"
    "9123456789"; // not just a memset, to ensure the allocator works correctly
  if (sizeof(written) < BUFFER_LENGTH)
  {
    fail("test error with string size");
    return NS_ERROR_FAILURE;
  }

  uint32_t writeCount;
  rv = output->Write(written, BUFFER_LENGTH, &writeCount);
  if (NS_FAILED(rv) || writeCount != BUFFER_LENGTH)
  {
    fail("writing %d bytes (wrote %d bytes) to output failed: %x",
         BUFFER_LENGTH, writeCount, rv);
    return rv;
  }

  char read[BUFFER_LENGTH];
  uint32_t readCount;
  rv = input->Read(read, BUFFER_LENGTH, &readCount);
  if (NS_FAILED(rv) || readCount != BUFFER_LENGTH)
  {
    fail("reading %d bytes (got %d bytes) from input failed: %x",
         BUFFER_LENGTH, readCount,  rv);
    return rv;
  }

  if (0 != memcmp(written, read, BUFFER_LENGTH))
  {
    fail("didn't read the written data correctly!");
    return NS_ERROR_FAILURE;
  }

  passed("TestPipe");
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("nsPipe");
  if (xpcom.failed())
    return 1;

  int rv = 0;

  if (NS_FAILED(TestPipe()))
    rv = 1;

  return rv;
}
