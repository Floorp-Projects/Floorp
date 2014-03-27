/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpdyZlibReporter.h"

namespace mozilla {

NS_IMPL_ISUPPORTS1(SpdyZlibReporter, nsIMemoryReporter)

/* static */ Atomic<size_t> SpdyZlibReporter::sAmount;

/* static */ void*
SpdyZlibReporter::Alloc(void*, uInt items, uInt size)
{
  void* p = moz_xmalloc(items * size);
  sAmount += MallocSizeOfOnAlloc(p);
  return p;
}

/* static */ void
SpdyZlibReporter::Free(void*, void* p)
{
  sAmount -= MallocSizeOfOnFree(p);
  moz_free(p);
}

NS_IMETHODIMP
SpdyZlibReporter::CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData)
{
  return MOZ_COLLECT_REPORT(
    "explicit/network/spdy-zlib-buffers", KIND_HEAP, UNITS_BYTES, sAmount,
    "Memory allocated for SPDY zlib send and receive buffers.");
}

} // namespace mozilla
