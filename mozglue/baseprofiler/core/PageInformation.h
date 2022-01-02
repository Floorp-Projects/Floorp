/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PageInformation_h
#define PageInformation_h

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"

#include <string>

namespace mozilla {
namespace baseprofiler {

class SpliceableJSONWriter;

// This class contains information that's relevant to a single page only
// while the page information is important and registered with the profiler,
// but regardless of whether the profiler is running. All accesses to it are
// protected by the profiler state lock.
// When the page gets unregistered, we keep the profiler buffer position
// to determine if we are still using this page. If not, we unregister
// it in the next page registration.
class PageInformation final {
 public:
  PageInformation(uint64_t aTabID, uint64_t aInnerWindowID,
                  const std::string& aUrl, uint64_t aEmbedderInnerWindowID);

  // Using hand-rolled ref-counting, because RefCounted.h macros don't produce
  // the same code between mozglue and libxul, see bug 1536656.
  MFBT_API void AddRef() const { ++mRefCnt; }
  MFBT_API void Release() const {
    MOZ_ASSERT(int32_t(mRefCnt) > 0);
    if (--mRefCnt) {
      delete this;
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
  bool Equals(PageInformation* aOtherPageInfo) const;
  void StreamJSON(SpliceableJSONWriter& aWriter) const;

  uint64_t InnerWindowID() const { return mInnerWindowID; }
  uint64_t TabID() const { return mTabID; }
  const std::string& Url() const { return mUrl; }
  uint64_t EmbedderInnerWindowID() const { return mEmbedderInnerWindowID; }

  Maybe<uint64_t> BufferPositionWhenUnregistered() const {
    return mBufferPositionWhenUnregistered;
  }

  void NotifyUnregistered(uint64_t aBufferPosition) {
    mBufferPositionWhenUnregistered = Some(aBufferPosition);
  }

 private:
  const uint64_t mTabID;
  const uint64_t mInnerWindowID;
  const std::string mUrl;
  const uint64_t mEmbedderInnerWindowID;

  // Holds the buffer position when page is unregistered.
  // It's used to determine if we still use this page in the profiler or
  // not.
  Maybe<uint64_t> mBufferPositionWhenUnregistered;

  mutable Atomic<int32_t, MemoryOrdering::ReleaseAcquire> mRefCnt;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // PageInformation_h
