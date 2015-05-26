/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsEntropyCollector.h"
#include "nsAlgorithm.h"
#include <algorithm>

nsEntropyCollector::nsEntropyCollector()
:mBytesCollected(0), mWritePointer(mEntropyCache)
{
  // We could use the uninitialized memory in mEntropyCache as initial
  // random data, but that means (if any entropy is collected before NSS
  // initialization and then forwarded) that we'll get warnings from
  // tools like valgrind for every later operation that depends on the
  // entropy.
  memset(mEntropyCache, 0, sizeof(mEntropyCache));
}

nsEntropyCollector::~nsEntropyCollector()
{
}

NS_IMPL_ISUPPORTS(nsEntropyCollector,
                  nsIEntropyCollector,
                  nsIBufEntropyCollector)

NS_IMETHODIMP
nsEntropyCollector::RandomUpdate(void *new_entropy, int32_t bufLen)
{
  if (bufLen > 0) {
    if (mForwardTarget) {
      return mForwardTarget->RandomUpdate(new_entropy, bufLen);
    }
    else {
      const unsigned char *InputPointer = (const unsigned char *)new_entropy;
      const unsigned char *PastEndPointer = mEntropyCache + entropy_buffer_size;

      // if the input is large, we only take as much as we can store
      int32_t bytes_wanted = std::min(bufLen, int32_t(entropy_buffer_size));

      // remember the number of bytes we will have after storing new_entropy
      mBytesCollected = std::min(int32_t(entropy_buffer_size),
                               mBytesCollected + bytes_wanted);

      // as the above statements limit bytes_wanted to the entropy_buffer_size,
      // this loop will iterate at most twice.
      while (bytes_wanted > 0) {

        // how many bytes to end of cyclic buffer?
        const int32_t space_to_end = PastEndPointer - mWritePointer;

        // how many bytes can we copy, not reaching the end of the buffer?
        const int32_t this_time = std::min(space_to_end, bytes_wanted);

        // copy at most to the end of the cyclic buffer
        for (int32_t i = 0; i < this_time; ++i) {

          unsigned int old = *mWritePointer;

          // combine new and old value already stored in buffer
          // this logic comes from PSM 1
          *mWritePointer++ = ((old << 1) | (old >> 7)) ^ *InputPointer++;
        }

        PR_ASSERT(mWritePointer <= PastEndPointer);
        PR_ASSERT(mWritePointer >= mEntropyCache);

        // have we arrived at the end of the buffer?
        if (PastEndPointer == mWritePointer) {
          // reset write pointer back to begining of our buffer
          mWritePointer = mEntropyCache;
        }

        // subtract the number of bytes we have already copied
        bytes_wanted -= this_time;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEntropyCollector::ForwardTo(nsIEntropyCollector *aCollector)
{
  NS_PRECONDITION(!mForwardTarget, "|ForwardTo| should only be called once.");

  mForwardTarget = aCollector;
  mForwardTarget->RandomUpdate(mEntropyCache, mBytesCollected);
  mBytesCollected = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsEntropyCollector::DontForward()
{
  mForwardTarget = nullptr;
  return NS_OK;
}
