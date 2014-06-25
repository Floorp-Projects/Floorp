/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A memory allocator for zlib use in SPDY that reports to about:memory. */

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "nsIMemoryReporter.h"
#include "zlib.h"

namespace mozilla {

class SpdyZlibReporter MOZ_FINAL : public nsIMemoryReporter
{
  ~SpdyZlibReporter() {}

public:
  NS_DECL_ISUPPORTS

  SpdyZlibReporter()
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount|
    // being static.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
    sAmount = 0;
  }

  static void* Alloc(void*, uInt items, uInt size);
  static void Free(void*, void* p);

private:
  // |sAmount| can be (implicitly) accessed by multiple threads, so it
  // must be thread-safe.
  static Atomic<size_t> sAmount;

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)
  MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(MallocSizeOfOnAlloc)
  MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE(MallocSizeOfOnFree)

  NS_IMETHODIMP
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize);
};

} // namespace mozilla
