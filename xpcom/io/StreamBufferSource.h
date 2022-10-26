/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StreamBufferSource_h
#define mozilla_StreamBufferSource_h

#include <cstddef>
#include "ErrorList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Span.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

namespace mozilla {

class StreamBufferSource {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StreamBufferSource)

  virtual Span<const char> Data() = 0;

  virtual nsresult GetData(nsACString& aString) {
    Span<const char> data = Data();
    if (!aString.Assign(data.Elements(), data.Length(), fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  virtual bool Owning() = 0;

  virtual size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) {
    return SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
  }
  size_t SizeOfIncludingThisIfUnshared(MallocSizeOf aMallocSizeOf) {
    if (mRefCnt > 1) {
      return 0;
    }
    size_t n = aMallocSizeOf(this);
    n += SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    return n;
  }

  virtual size_t SizeOfExcludingThisEvenIfShared(
      MallocSizeOf aMallocSizeOf) = 0;
  size_t SizeOfIncludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) {
    size_t n = aMallocSizeOf(this);
    n += SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
    return n;
  }

 protected:
  virtual ~StreamBufferSource() = default;
};

}  // namespace mozilla

#endif  // mozilla_StreamBufferSource_h
