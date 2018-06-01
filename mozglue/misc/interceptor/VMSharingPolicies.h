/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_VMSharingPolicies_h
#define mozilla_interceptor_VMSharingPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

namespace mozilla {
namespace interceptor {

template <typename MMPolicy, uint32_t kChunkSize>
class VMSharingPolicyUnique : public MMPolicy
{
public:
  template <typename... Args>
  explicit VMSharingPolicyUnique(Args... aArgs)
    : MMPolicy(mozilla::Forward<Args>(aArgs)...)
    , mNextChunkIndex(0)
  {
  }

  bool Reserve(uint32_t aCount)
  {
    MOZ_ASSERT(aCount);
    uint32_t bytesReserved = MMPolicy::Reserve(aCount * kChunkSize);
    return !!bytesReserved;
  }

  Trampoline<MMPolicy> GetNextTrampoline()
  {
    uint32_t offset = mNextChunkIndex * kChunkSize;
    if (!MaybeCommitNextPage(offset, kChunkSize)) {
      return nullptr;
    }


    Trampoline<MMPolicy> result(this, GetLocalView() + offset,
                                GetRemoteView() + offset, kChunkSize);
    if (!!result) {
      ++mNextChunkIndex;
    }

    return Move(result);
  }

  TrampolineCollection<MMPolicy> Items() const
  {
    return TrampolineCollection<MMPolicy>(*this, GetLocalView(), GetRemoteView(),
                                          kChunkSize, mNextChunkIndex);
  }

  void Clear()
  {
    mNextChunkIndex = 0;
  }

  ~VMSharingPolicyUnique() = default;

  VMSharingPolicyUnique(const VMSharingPolicyUnique&) = delete;
  VMSharingPolicyUnique& operator=(const VMSharingPolicyUnique&) = delete;

  VMSharingPolicyUnique(VMSharingPolicyUnique&& aOther)
    : MMPolicy(Move(aOther))
    , mNextChunkIndex(aOther.mNextChunkIndex)
  {
    aOther.mNextChunkIndex = 0;
  }

  VMSharingPolicyUnique& operator=(VMSharingPolicyUnique&& aOther)
  {
    static_cast<MMPolicy&>(*this) = Move(aOther);
    mNextChunkIndex = aOther.mNextChunkIndex;
    aOther.mNextChunkIndex = 0;
    return *this;
  }

private:
  uint32_t  mNextChunkIndex;
};

template <typename MMPolicy, uint32_t kChunkSize>
class VMSharingPolicyShared;

// We only support this policy for in-proc MMPolicy
template <uint32_t kChunkSize>
class VMSharingPolicyShared<MMPolicyInProcess, kChunkSize> : public MMPolicyBase
{
  typedef VMSharingPolicyUnique<MMPolicyInProcess, kChunkSize> UniquePolicyT;

public:
  typedef MMPolicyInProcess MMPolicyT;

  VMSharingPolicyShared()
  {
    static const bool isAlloc = []() -> bool {
      DWORD flags = 0;
#if defined(RELEASE_OR_BETA)
      flags |= CRITICAL_SECTION_NO_DEBUG_INFO;
#endif // defined(RELEASE_OR_BETA)
      ::InitializeCriticalSectionEx(&sCS, 4000, flags);
      return true;
    }();
  }

  explicit operator bool() const
  {
    AutoCriticalSection lock(&sCS);
    return !!sUniqueVM;
  }

  operator const MMPolicyInProcess&() const
  {
    AutoCriticalSection lock(&sCS);
    return sUniqueVM;
  }

  bool ShouldUnhookUponDestruction() const
  {
    AutoCriticalSection lock(&sCS);
    return sUniqueVM.ShouldUnhookUponDestruction();
  }

  bool Reserve(uint32_t aCount)
  {
    AutoCriticalSection lock(&sCS);
    return sUniqueVM.Reserve(aCount);
  }

  bool IsPageAccessible(void* aVAddress) const
  {
    AutoCriticalSection lock(&sCS);
    return sUniqueVM.IsPageAccessible(aVAddress);
  }

  Trampoline<MMPolicyInProcess> GetNextTrampoline()
  {
    AutoCriticalSection lock(&sCS);
    return sUniqueVM.GetNextTrampoline();
  }

  TrampolineCollection<MMPolicyInProcess> Items() const
  {
    AutoCriticalSection lock(&sCS);
    TrampolineCollection<MMPolicyInProcess> items(Move(sUniqueVM.Items()));

    // We need to continue holding the lock until items is destroyed.
    items.Lock(sCS);

    return Move(items);
  }

  void Clear()
  {
    // This must be a no-op for shared VM policy; we can't have one interceptor
    // wiping out trampolines for all interceptors in the process.
  }

  ~VMSharingPolicyShared() = default;

  VMSharingPolicyShared(const VMSharingPolicyShared&) = delete;
  VMSharingPolicyShared(VMSharingPolicyShared&&) = delete;
  VMSharingPolicyShared& operator=(const VMSharingPolicyShared&) = delete;
  VMSharingPolicyShared& operator=(VMSharingPolicyShared&&) = delete;

private:
  static UniquePolicyT sUniqueVM;
  static CRITICAL_SECTION sCS;
};

template <uint32_t kChunkSize>
typename VMSharingPolicyShared<MMPolicyInProcess, kChunkSize>::UniquePolicyT
  VMSharingPolicyShared<MMPolicyInProcess, kChunkSize>::sUniqueVM;

template <uint32_t kChunkSize>
CRITICAL_SECTION VMSharingPolicyShared<MMPolicyInProcess, kChunkSize>::sCS;

} // namespace interceptor
} // namespace mozilla

#endif // mozilla_interceptor_VMSharingPolicies_h

