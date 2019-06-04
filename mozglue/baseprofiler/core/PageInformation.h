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
  PageInformation(const std::string& aDocShellId, uint32_t aDocShellHistoryId,
                  const std::string& aUrl, bool aIsSubFrame);

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
  bool Equals(PageInformation* aOtherDocShellInfo);
  void StreamJSON(SpliceableJSONWriter& aWriter);

  uint32_t DocShellHistoryId() { return mDocShellHistoryId; }
  const std::string& DocShellId() { return mDocShellId; }
  const std::string& Url() { return mUrl; }
  bool IsSubFrame() { return mIsSubFrame; }

  Maybe<uint64_t> BufferPositionWhenUnregistered() {
    return mBufferPositionWhenUnregistered;
  }

  void NotifyUnregistered(uint64_t aBufferPosition) {
    mBufferPositionWhenUnregistered = Some(aBufferPosition);
  }

 private:
  const std::string mDocShellId;
  const uint32_t mDocShellHistoryId;
  const std::string mUrl;
  const bool mIsSubFrame;

  // Holds the buffer position when DocShell is unregistered.
  // It's used to determine if we still use this DocShell in the profiler or
  // not.
  Maybe<uint64_t> mBufferPositionWhenUnregistered;

  mutable Atomic<int32_t, MemoryOrdering::ReleaseAcquire,
                 recordreplay::Behavior::DontPreserve>
      mRefCnt;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // PageInformation_h
