/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_MMPolicies_h
#define mozilla_interceptor_MMPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#include <windows.h>

namespace mozilla {
namespace interceptor {

class MMPolicyBase
{
public:
  static DWORD ComputeAllocationSize(const uint32_t aRequestedSize)
  {
    MOZ_ASSERT(aRequestedSize);
    DWORD result = aRequestedSize;

    const uint32_t granularity = GetAllocGranularity();

    uint32_t mod = aRequestedSize % granularity;
    if (mod) {
      result += (granularity - mod);
    }

    return result;
  }

  static DWORD GetAllocGranularity()
  {
    static const DWORD kAllocGranularity = []() -> DWORD {
      SYSTEM_INFO sysInfo;
      ::GetSystemInfo(&sysInfo);
      return sysInfo.dwAllocationGranularity;
    }();

    return kAllocGranularity;
  }

  static DWORD GetPageSize()
  {
    static const DWORD kPageSize = []() -> DWORD {
      SYSTEM_INFO sysInfo;
      ::GetSystemInfo(&sysInfo);
      return sysInfo.dwPageSize;
    }();

    return kPageSize;
  }
};

class MMPolicyInProcess : public MMPolicyBase
{
public:
  typedef MMPolicyInProcess MMPolicyT;

  explicit MMPolicyInProcess()
    : mBase(nullptr)
    , mReservationSize(0)
    , mCommitOffset(0)
  {
  }

  MMPolicyInProcess(const MMPolicyInProcess&) = delete;
  MMPolicyInProcess& operator=(const MMPolicyInProcess&) = delete;

  MMPolicyInProcess(MMPolicyInProcess&& aOther)
    : mBase(nullptr)
    , mReservationSize(0)
    , mCommitOffset(0)
  {
    *this = Move(aOther);
  }

  MMPolicyInProcess& operator=(MMPolicyInProcess&& aOther)
  {
    mBase = aOther.mBase;
    aOther.mBase = nullptr;

    mCommitOffset = aOther.mCommitOffset;
    aOther.mCommitOffset = 0;

    mReservationSize = aOther.mReservationSize;
    aOther.mReservationSize = 0;

    return *this;
  }

  // We always leak mBase
  ~MMPolicyInProcess() = default;

  explicit operator bool() const
  {
    return !!mBase;
  }

  /**
   * Should we unhook everything upon destruction?
   */
  bool ShouldUnhookUponDestruction() const
  {
    return true;
  }

  bool Read(void* aToPtr, const void* aFromPtr, size_t aLen) const
  {
    ::memcpy(aToPtr, aFromPtr, aLen);
    return true;
  }

  bool Write(void* aToPtr, const void* aFromPtr, size_t aLen) const
  {
    ::memcpy(aToPtr, aFromPtr, aLen);
    return true;
  }

  bool Protect(void* aVAddress, size_t aSize, uint32_t aProtFlags,
               uint32_t* aPrevProtFlags) const
  {
    MOZ_ASSERT(aPrevProtFlags);
    BOOL ok = ::VirtualProtect(aVAddress, aSize, aProtFlags,
                               reinterpret_cast<PDWORD>(aPrevProtFlags));
    MOZ_ASSERT(ok);
    return !!ok;
  }

  /**
   * @return true if the page that hosts aVAddress is accessible.
   */
  bool IsPageAccessible(void* aVAddress) const
  {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = ::VirtualQuery(aVAddress, &mbi, sizeof(mbi));

    return result && mbi.AllocationProtect && (mbi.Type & MEM_IMAGE) &&
           mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS;
  }

  bool FlushInstructionCache() const
  {
    return !!::FlushInstructionCache(::GetCurrentProcess(), nullptr, 0);
  }

protected:
  uint8_t* GetLocalView() const
  {
    return mBase;
  }

  uintptr_t GetRemoteView() const
  {
    // Same as local view for in-process
    return reinterpret_cast<uintptr_t>(mBase);
  }

  /**
   * @return the effective number of bytes reserved, or 0 on failure
   */
  uint32_t Reserve(const uint32_t aSize)
  {
    if (!aSize) {
      return 0;
    }

    if (mBase) {
      MOZ_ASSERT(mReservationSize >= aSize);
      return mReservationSize;
    }

    mReservationSize = ComputeAllocationSize(aSize);
    mBase = static_cast<uint8_t*>(::VirtualAlloc(nullptr, mReservationSize,
                                                 MEM_RESERVE, PAGE_NOACCESS));
    if (!mBase) {
      return 0;
    }
    return mReservationSize;
  }

  bool MaybeCommitNextPage(const uint32_t aRequestedOffset,
                           const uint32_t aRequestedLength)
  {
    if (!(*this)) {
      return false;
    }

    uint32_t limit = aRequestedOffset + aRequestedLength - 1;
    if (limit < mCommitOffset) {
      // No commit required
      return true;
    }

    MOZ_DIAGNOSTIC_ASSERT(mCommitOffset < mReservationSize);
    if (mCommitOffset >= mReservationSize) {
      return false;
    }

    PVOID local = ::VirtualAlloc(mBase + mCommitOffset, GetPageSize(),
                                 MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!local) {
      return false;
    }

    mCommitOffset += GetPageSize();
    return true;
  }

private:
  uint8_t*  mBase;
  uint32_t  mReservationSize;
  uint32_t  mCommitOffset;
};

} // namespace interceptor
} // namespace mozilla

#endif // mozilla_interceptor_MMPolicies_h

