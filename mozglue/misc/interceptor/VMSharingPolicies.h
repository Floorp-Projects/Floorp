/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_VMSharingPolicies_h
#define mozilla_interceptor_VMSharingPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Types.h"

namespace mozilla {
namespace interceptor {

/**
 * This class is an abstraction of a reservation of virtual address space that
 * has been obtained from a VMSharingPolicy via the policy's |Reserve| method.
 *
 * TrampolinePool allows us to obtain a trampoline without needing to concern
 * ourselves with the underlying implementation of the VM sharing policy.
 *
 * For example, VMSharingPolicyShared delegates to VMSharingPolicyUnique, but
 * also requires taking a lock before doing so. By invoking |GetNextTrampoline|
 * on a TrampolinePool, the caller does not need to concern themselves with
 * that detail.
 *
 * We accompolish this with a recursive implementation that provides an inner
 * TrampolinePool that is provided by the delegated VMSharingPolicy.
 */
template <typename VMPolicyT, typename InnerT>
class MOZ_STACK_CLASS TrampolinePool final {
 public:
  TrampolinePool(TrampolinePool&& aOther) = default;

  TrampolinePool(VMPolicyT& aVMPolicy, InnerT&& aInner)
      : mVMPolicy(aVMPolicy), mInner(std::move(aInner)) {}

  TrampolinePool& operator=(TrampolinePool&& aOther) = delete;
  TrampolinePool(const TrampolinePool&) = delete;
  TrampolinePool& operator=(const TrampolinePool&) = delete;

  using MMPolicyT = typename VMPolicyT::MMPolicyT;

  Maybe<Trampoline<MMPolicyT>> GetNextTrampoline() {
    return mVMPolicy.GetNextTrampoline(mInner);
  }

#if defined(_M_X64)
  bool IsInLowest2GB() const {
    return mVMPolicy.IsTrampolineSpaceInLowest2GB(mInner);
  }
#endif  // defined(_M_X64)

 private:
  VMPolicyT& mVMPolicy;
  InnerT mInner;
};

/**
 * This specialization is the base case for TrampolinePool, and is used by
 * VMSharingPolicyUnique (since that policy does not delegate anything).
 */
template <typename VMPolicyT>
class MOZ_STACK_CLASS TrampolinePool<VMPolicyT, decltype(nullptr)> final {
 public:
  explicit TrampolinePool(VMPolicyT& aVMPolicy) : mVMPolicy(aVMPolicy) {}

  TrampolinePool(TrampolinePool&& aOther) = default;

  TrampolinePool& operator=(TrampolinePool&& aOther) = delete;
  TrampolinePool(const TrampolinePool&) = delete;
  TrampolinePool& operator=(const TrampolinePool&) = delete;

  using MMPolicyT = typename VMPolicyT::MMPolicyT;

  Maybe<Trampoline<MMPolicyT>> GetNextTrampoline() {
    return mVMPolicy.GetNextTrampoline();
  }

#if defined(_M_X64)
  bool IsInLowest2GB() const {
    return mVMPolicy.IsTrampolineSpaceInLowest2GB();
  }
#endif  // defined(_M_X64)

 private:
  VMPolicyT& mVMPolicy;
};

template <typename MMPolicy>
class VMSharingPolicyUnique : public MMPolicy {
  using ThisType = VMSharingPolicyUnique<MMPolicy>;

 public:
  using PoolType = TrampolinePool<ThisType, decltype(nullptr)>;

  template <typename... Args>
  explicit VMSharingPolicyUnique(Args&&... aArgs)
      : MMPolicy(std::forward<Args>(aArgs)...), mNextChunkIndex(0) {}

  Maybe<PoolType> Reserve(const uintptr_t aPivotAddr,
                          const uint32_t aMaxDistanceFromPivot) {
    // Win32 allocates VM addresses at a 64KiB granularity, so we might as well
    // utilize that entire 64KiB reservation.
    uint32_t len = MMPolicy::GetAllocGranularity();

    Maybe<Span<const uint8_t>> maybeBounds = MMPolicy::SpanFromPivotAndDistance(
        len, aPivotAddr, aMaxDistanceFromPivot);

    return Reserve(len, maybeBounds);
  }

  Maybe<PoolType> Reserve(const uint32_t aSize,
                          const Maybe<Span<const uint8_t>>& aBounds) {
    uint32_t bytesReserved = MMPolicy::Reserve(aSize, aBounds);
    if (!bytesReserved) {
      return Nothing();
    }

    return Some(PoolType(*this));
  }

  TrampolineCollection<MMPolicy> Items() const {
    return TrampolineCollection<MMPolicy>(*this, this->GetLocalView(),
                                          this->GetRemoteView(), kChunkSize,
                                          mNextChunkIndex);
  }

  void Clear() { mNextChunkIndex = 0; }

  ~VMSharingPolicyUnique() = default;

  VMSharingPolicyUnique(const VMSharingPolicyUnique&) = delete;
  VMSharingPolicyUnique& operator=(const VMSharingPolicyUnique&) = delete;

  VMSharingPolicyUnique(VMSharingPolicyUnique&& aOther)
      : MMPolicy(std::move(aOther)), mNextChunkIndex(aOther.mNextChunkIndex) {
    aOther.mNextChunkIndex = 0;
  }

  VMSharingPolicyUnique& operator=(VMSharingPolicyUnique&& aOther) {
    static_cast<MMPolicy&>(*this) = std::move(aOther);
    mNextChunkIndex = aOther.mNextChunkIndex;
    aOther.mNextChunkIndex = 0;
    return *this;
  }

 protected:
  // In VMSharingPolicyUnique we do not implement the overload that accepts
  // an inner trampoline pool, as this policy is expected to be the
  // implementation of the base case.
  Maybe<Trampoline<MMPolicy>> GetNextTrampoline() {
    uint32_t offset = mNextChunkIndex * kChunkSize;
    if (!this->MaybeCommitNextPage(offset, kChunkSize)) {
      return Nothing();
    }

    Trampoline<MMPolicy> result(this, this->GetLocalView() + offset,
                                this->GetRemoteView() + offset, kChunkSize);
    if (!!result) {
      ++mNextChunkIndex;
    }

    return Some(std::move(result));
  }

 private:
  uint32_t mNextChunkIndex;
  static const uint32_t kChunkSize = 128;

  template <typename VMPolicyT, typename FriendT>
  friend class TrampolinePool;
};

}  // namespace interceptor
}  // namespace mozilla

// We don't include RangeMap.h until this point because it depends on the
// TrampolinePool definitions from above.
#include "mozilla/interceptor/RangeMap.h"

namespace mozilla {
namespace interceptor {

template <typename MMPolicy, bool Dummy>
class VMSharingPolicyShared;

// We only support this policy for in-proc MMPolicy.
// Dummy is not actually needed for the implementation, but we need this
// to be a partial specialization so that its statics are treated as inlines
// (At least until we have C++17 enabled by default)
template <bool Dummy>
class MOZ_TRIVIAL_CTOR_DTOR VMSharingPolicyShared<MMPolicyInProcess, Dummy>
    : public MMPolicyInProcess {
  typedef VMSharingPolicyUnique<MMPolicyInProcess> UniquePolicyT;
  typedef VMSharingPolicyShared<MMPolicyInProcess, Dummy> ThisType;

 public:
  using PoolType = TrampolinePool<ThisType, UniquePolicyT::PoolType>;
  using MMPolicyT = MMPolicyInProcess;

  constexpr VMSharingPolicyShared() {}

  bool ShouldUnhookUponDestruction() const { return false; }

  Maybe<PoolType> Reserve(const uintptr_t aPivotAddr,
                          const uint32_t aMaxDistanceFromPivot) {
    // Win32 allocates VM addresses at a 64KiB granularity, so we might as well
    // utilize that entire 64KiB reservation.
    uint32_t len = this->GetAllocGranularity();

    Maybe<Span<const uint8_t>> maybeBounds =
        MMPolicyInProcess::SpanFromPivotAndDistance(len, aPivotAddr,
                                                    aMaxDistanceFromPivot);

    AutoCriticalSection lock(GetCS());
    VMSharingPolicyUnique<MMPolicyT>* uniquePol = sVMMap.GetPolicy(maybeBounds);
    MOZ_ASSERT(uniquePol);
    if (!uniquePol) {
      return Nothing();
    }

    Maybe<UniquePolicyT::PoolType> maybeUnique =
        uniquePol->Reserve(len, maybeBounds);
    if (!maybeUnique) {
      return Nothing();
    }

    return Some(PoolType(*this, std::move(maybeUnique.ref())));
  }

  TrampolineCollection<MMPolicyInProcess> Items() const {
    // Since ShouldUnhookUponDestruction returns false, this can be empty
    return TrampolineCollection<MMPolicyInProcess>(*this);
  }

  void Clear() {
    // This must be a no-op for shared VM policy; we can't have one interceptor
    // wiping out trampolines for all interceptors in the process.
  }

  VMSharingPolicyShared(const VMSharingPolicyShared&) = delete;
  VMSharingPolicyShared(VMSharingPolicyShared&&) = delete;
  VMSharingPolicyShared& operator=(const VMSharingPolicyShared&) = delete;
  VMSharingPolicyShared& operator=(VMSharingPolicyShared&&) = delete;

 private:
  static CRITICAL_SECTION* GetCS() {
    static const bool isAlloc = []() -> bool {
      DWORD flags = 0;
#if defined(RELEASE_OR_BETA)
      flags |= CRITICAL_SECTION_NO_DEBUG_INFO;
#endif  // defined(RELEASE_OR_BETA)
      ::InitializeCriticalSectionEx(&sCS, 4000, flags);
      return true;
    }();
    Unused << isAlloc;

    return &sCS;
  }

  // In VMSharingPolicyShared, we only implement the overload that accepts
  // a VMSharingPolicyUnique trampoline pool as |aInner|, since we require the
  // former policy to wrap the latter.
  Maybe<Trampoline<MMPolicyInProcess>> GetNextTrampoline(
      UniquePolicyT::PoolType& aInner) {
    AutoCriticalSection lock(GetCS());
    return aInner.GetNextTrampoline();
  }

#if defined(_M_X64)
  bool IsTrampolineSpaceInLowest2GB(
      const UniquePolicyT::PoolType& aInner) const {
    AutoCriticalSection lock(GetCS());
    return aInner.IsInLowest2GB();
  }
#endif  // defined(_M_X64)

 private:
  template <typename VMPolicyT, typename InnerT>
  friend class TrampolinePool;

  static RangeMap<MMPolicyInProcess> sVMMap;
  static CRITICAL_SECTION sCS;
};

template <bool Dummy>
RangeMap<MMPolicyInProcess>
    VMSharingPolicyShared<MMPolicyInProcess, Dummy>::sVMMap;

template <bool Dummy>
CRITICAL_SECTION VMSharingPolicyShared<MMPolicyInProcess, Dummy>::sCS;

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_VMSharingPolicies_h
