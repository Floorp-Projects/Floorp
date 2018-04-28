/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_VMSharingPolicies_h
#define mozilla_interceptor_VMSharingPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"
#include "mozilla/StaticPtr.h"

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
class VMSharingPolicyShared : public MMPolicyBase
{
  typedef VMSharingPolicyUnique<MMPolicy, kChunkSize> ValueT;

  // We use pid instead of HANDLE for mapping, since more than one handle may
  // map to the same pid. We don't worry about pid reuse becuase each mVMPolicy
  // holds an open handle to pid, thus keeping the pid reserved at least for the
  // lifetime of mVMPolicy.
  struct ProcMapEntry
  {
    ProcMapEntry()
      : mPid(::GetCurrentProcessId())
    {
    }

    explicit ProcMapEntry(HANDLE aProc)
      : mPid(::GetProcessId(aProc))
      , mVMPolicy(aProc)
    {
    }

    ProcMapEntry(ProcMapEntry&& aOther)
      : mPid(aOther.mPid)
      , mVMPolicy(Move(aOther.mVMPolicy))
    {
      aOther.mPid = 0;
    }

    ProcMapEntry(const ProcMapEntry&) = delete;
    ProcMapEntry& operator=(const ProcMapEntry&) = delete;

    ProcMapEntry& operator=(ProcMapEntry&& aOther)
    {
      mPid = aOther.mPid;
      mVMPolicy = Move(aOther.mVMPolicy);
      aOther.mPid = 0;
      return *this;
    }

    bool operator==(DWORD aPid) const
    {
      return mPid == aPid;
    }

    DWORD   mPid;
    ValueT  mVMPolicy;
  };

  // We normally expect to reference only one other process at a time, but this
  // is not a requirement.
  typedef Vector<ProcMapEntry, 1> MapT;

public:
  typedef MMPolicy MMPolicyT;

  template <typename... Args>
  explicit VMSharingPolicyShared(Args... aArgs)
    : mPid(GetPid(aArgs...))
  {
    static const bool isAlloc = []() -> bool {
      sPerProcVM = new MapT();
      DWORD flags = 0;
#if defined(RELEASE_OR_BETA)
      flags |= CRITICAL_SECTION_NO_DEBUG_INFO;
#endif // defined(RELEASE_OR_BETA)
      ::InitializeCriticalSectionEx(&sCS, 4000, flags);
      return true;
    }();

    MOZ_ASSERT(mPid);
    if (!mPid) {
      return;
    }

    AutoCriticalSection lock(&sCS);

    if (find(mPid)) {
      return;
    }

    MOZ_RELEASE_ASSERT(sPerProcVM->append(ProcMapEntry(aArgs...)));
  }

  explicit operator bool() const
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    MOZ_RELEASE_ASSERT(find(mPid, &entry));

    return !!entry->mVMPolicy;
  }

  operator const MMPolicy&() const
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    MOZ_RELEASE_ASSERT(find(mPid, &entry));

    return entry->mVMPolicy;
  }

  bool ShouldUnhookUponDestruction() const
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    if (!find(mPid, &entry)) {
      return 0;
    }

    return entry->mVMPolicy.ShouldUnhookUponDestruction();
  }

  bool Reserve(uint32_t aCount)
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    if (!find(mPid, &entry)) {
      return false;
    }

    return entry->mVMPolicy.Reserve(aCount);
  }

  bool IsPageAccessible(void* aVAddress) const
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    if (!find(mPid, &entry)) {
      return false;
    }

    return entry->mVMPolicy.IsPageAccessible(aVAddress);
  }

  Trampoline<MMPolicy> GetNextTrampoline()
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    if (!find(mPid, &entry)) {
      return nullptr;
    }

    return entry->mVMPolicy.GetNextTrampoline();
  }

  TrampolineCollection<MMPolicy> Items() const
  {
    AutoCriticalSection lock(&sCS);

    ProcMapEntry* entry;
    MOZ_RELEASE_ASSERT(find(mPid, &entry));

    TrampolineCollection<MMPolicy> items(Move(entry->mVMPolicy.Items()));

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
  static bool find(DWORD aPid, ProcMapEntry** aOutEntry = nullptr)
  {
    MOZ_ASSERT(sPerProcVM);
    if (!sPerProcVM) {
      return false;
    }

    if (aOutEntry) {
      *aOutEntry = nullptr;
    }

    for (auto&& mapping : *sPerProcVM) {
      if (mapping == aPid) {
        if (aOutEntry) {
          *aOutEntry = &mapping;
        }
        return true;
      }
    }

    return false;
  }

  static DWORD GetPid() { return ::GetCurrentProcessId(); }
  static DWORD GetPid(HANDLE aHandle) { return ::GetProcessId(aHandle); }

  DWORD mPid;
  static StaticAutoPtr<MapT> sPerProcVM;
  static CRITICAL_SECTION sCS;
};

template <typename MMPolicy, uint32_t kChunkSize>
StaticAutoPtr<typename VMSharingPolicyShared<MMPolicy, kChunkSize>::MapT>
  VMSharingPolicyShared<MMPolicy, kChunkSize>::sPerProcVM;

template <typename MMPolicy, uint32_t kChunkSize>
CRITICAL_SECTION VMSharingPolicyShared<MMPolicy, kChunkSize>::sCS;

} // namespace interceptor
} // namespace mozilla

#endif // mozilla_interceptor_VMSharingPolicies_h

