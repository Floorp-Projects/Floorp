/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIPipe.h"
#include "nsIMemory.h"
#include "mozilla/Attributes.h"

/** NS_NewPipe2 reimplemented, because it's not exported by XPCOM */
nsresult TP_NewPipe2(nsIAsyncInputStream** input,
                     nsIAsyncOutputStream** output,
                     bool nonBlockingInput,
                     bool nonBlockingOutput,
                     uint32_t segmentSize,
                     uint32_t segmentCount,
                     nsIMemory* segmentAlloc)
{
  nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
  if (!pipe)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = pipe->Init(nonBlockingInput,
                           nonBlockingOutput,
                           segmentSize,
                           segmentCount,
                           segmentAlloc);

  if (NS_FAILED(rv))
    return rv;

  pipe->GetInputStream(input);
  pipe->GetOutputStream(output);
  return NS_OK;
}

/**
 * Allocator can allocate exactly count * size bytes, stored at mMemory;
 * immediately after the end of this is a byte-map of 0/1 values indicating
 * which <size>-byte locations in mMemory are empty and which are filled.
 * Pretty stupid, but enough to test bug 394692.
 */
class BackwardsAllocator MOZ_FINAL : public nsIMemory
{
  public:
    BackwardsAllocator()
      : mMemory(0),
        mIndex(0xFFFFFFFF),
        mCount(0xFFFFFFFF),
        mSize(0)
    { }
    ~BackwardsAllocator()
    {
      delete [] mMemory;
    }

    nsresult Init(uint32_t count, size_t size);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORY

  private:
    uint32_t previous(uint32_t i)
    {
      if (i == 0)
        return mCount - 1;
      return i - 1;
    }

  private:
    uint8_t* mMemory;
    uint32_t mIndex;
    uint32_t mCount;
    size_t mSize;
};

NS_IMPL_ISUPPORTS1(BackwardsAllocator, nsIMemory)

nsresult BackwardsAllocator::Init(uint32_t count, size_t size)
{
  if (mMemory)
  {
    fail("allocator already initialized!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mMemory = new uint8_t[count * size + count];
  if (!mMemory)
  {
    fail("failed to allocate mMemory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(mMemory, 0, count * size + count);

  mIndex = 0;
  mCount = count;
  mSize = size;

  return NS_OK;
}

NS_IMETHODIMP_(void*) BackwardsAllocator::Alloc(size_t size)
{
  if (size != mSize)
  {
    NS_ERROR("umm, why would this be reached for this test?");
    return NULL;
  }

  uint32_t index = mIndex;

  while ((index = previous(index)) != mIndex)
  {
    if (mMemory[mSize * mCount + index] == 1)
      continue;
    mMemory[mSize * mCount + index] = 1;
    mIndex = index;
    return &mMemory[mSize * index];
  }

  NS_ERROR("shouldn't reach here in this test");
  return NULL;
}

NS_IMETHODIMP_(void*) BackwardsAllocator::Realloc(void* ptr, size_t newSize)
{
  NS_ERROR("shouldn't reach here in this test");
  return NULL;
}

NS_IMETHODIMP_(void) BackwardsAllocator::Free(void* ptr)
{
  uint8_t* p = static_cast<uint8_t*>(ptr);
  if (p)
    mMemory[mCount * mSize + (p - mMemory) / mSize] = 0;
}

NS_IMETHODIMP BackwardsAllocator::HeapMinimize(bool immediate)
{
  return NS_OK;
}

NS_IMETHODIMP BackwardsAllocator::IsLowMemory(bool* retval)
{
  *retval = false;
  return NS_OK;
}

NS_IMETHODIMP BackwardsAllocator::IsLowMemoryPlatform(bool* retval)
{
  *retval = false;
  return NS_OK;
}

nsresult TestBackwardsAllocator()
{
  const uint32_t SEGMENT_COUNT = 10;
  const uint32_t SEGMENT_SIZE = 10;

  nsRefPtr<BackwardsAllocator> allocator = new BackwardsAllocator();
  if (!allocator)
  {
    fail("Allocation of BackwardsAllocator failed!");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = allocator->Init(SEGMENT_COUNT, SEGMENT_SIZE);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIAsyncInputStream> input;
  nsCOMPtr<nsIAsyncOutputStream> output;
  rv = TP_NewPipe2(getter_AddRefs(input),
                   getter_AddRefs(output),
                   false,
                   false,
                   SEGMENT_SIZE, SEGMENT_COUNT, allocator); 
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

  passed("TestBackwardsAllocator");
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("nsPipe");
  if (xpcom.failed())
    return 1;

  int rv = 0;

  if (NS_FAILED(TestBackwardsAllocator()))
    rv = 1;

  return rv;
}
