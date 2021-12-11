/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_RangeMap_h
#define mozilla_interceptor_RangeMap_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include <algorithm>

namespace mozilla {
namespace interceptor {

/**
 * This class maintains a vector of VMSharingPolicyUnique objects, sorted on
 * the memory range that is used for reserving each object.
 *
 * This is used by VMSharingPolicyShared for creating and looking up VM regions
 * that are within proximity of the applicable range.
 *
 * VMSharingPolicyUnique objects managed by this class are reused whenever
 * possible. If no range is required, we just return the first available
 * policy.
 *
 * If no range is required and no policies have yet been allocated, we create
 * a new one with a null range as a default.
 */
template <typename MMPolicyT>
class RangeMap final {
 private:
  /**
   * This class is used as the comparison key for sorting and insertion.
   */
  class Range {
   public:
    constexpr Range() : mBase(0), mLimit(0) {}

    explicit Range(const Maybe<Span<const uint8_t>>& aBounds)
        : mBase(aBounds ? reinterpret_cast<const uintptr_t>(
                              MMPolicyT::GetLowerBound(aBounds.ref()))
                        : 0),
          mLimit(aBounds ? reinterpret_cast<const uintptr_t>(
                               MMPolicyT::GetUpperBoundIncl(aBounds.ref()))
                         : 0) {}

    Range& operator=(const Range&) = default;
    Range(const Range&) = default;
    Range(Range&&) = default;
    Range& operator=(Range&&) = default;

    bool operator<(const Range& aOther) const {
      return mBase < aOther.mBase ||
             (mBase == aOther.mBase && mLimit < aOther.mLimit);
    }

    bool Contains(const Range& aOther) const {
      return mBase <= aOther.mBase && mLimit >= aOther.mLimit;
    }

   private:
    uintptr_t mBase;
    uintptr_t mLimit;
  };

  class PolicyInfo final : public Range {
   public:
    explicit PolicyInfo(const Range& aRange)
        : Range(aRange),
          mPolicy(MakeUnique<VMSharingPolicyUnique<MMPolicyT>>()) {}

    PolicyInfo(const PolicyInfo&) = delete;
    PolicyInfo& operator=(const PolicyInfo&) = delete;

    PolicyInfo(PolicyInfo&& aOther) = default;
    PolicyInfo& operator=(PolicyInfo&& aOther) = default;

    VMSharingPolicyUnique<MMPolicyT>* GetPolicy() { return mPolicy.get(); }

   private:
    UniquePtr<VMSharingPolicyUnique<MMPolicyT>> mPolicy;
  };

  using VectorType = Vector<PolicyInfo, 0, InfallibleAllocPolicy>;

 public:
  constexpr RangeMap() : mPolicies(nullptr) {}

  VMSharingPolicyUnique<MMPolicyT>* GetPolicy(
      const Maybe<Span<const uint8_t>>& aBounds) {
    Range testRange(aBounds);

    if (!mPolicies) {
      mPolicies = new VectorType();
    }

    // If no bounds are specified, we just use the first available policy
    if (!aBounds) {
      if (mPolicies->empty()) {
        if (!mPolicies->append(PolicyInfo(testRange))) {
          return nullptr;
        }
      }

      return GetFirstPolicy();
    }

    // mPolicies is sorted, so we search
    auto itr =
        std::lower_bound(mPolicies->begin(), mPolicies->end(), testRange);
    if (itr != mPolicies->end() && itr->Contains(testRange)) {
      return itr->GetPolicy();
    }

    itr = mPolicies->insert(itr, PolicyInfo(testRange));

    MOZ_ASSERT(std::is_sorted(mPolicies->begin(), mPolicies->end()));

    return itr->GetPolicy();
  }

 private:
  VMSharingPolicyUnique<MMPolicyT>* GetFirstPolicy() {
    MOZ_RELEASE_ASSERT(mPolicies && !mPolicies->empty());
    return mPolicies->begin()->GetPolicy();
  }

 private:
  VectorType* mPolicies;
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_RangeMap_h
