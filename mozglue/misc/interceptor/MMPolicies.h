/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_MMPolicies_h
#define mozilla_interceptor_MMPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsMapRemoteView.h"

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
    if (!ok && aPrevProtFlags) {
      // VirtualProtect can fail but still set valid protection flags.
      // Let's clear those upon failure.
      *aPrevProtFlags = 0;
    }

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

class MMPolicyOutOfProcess : public MMPolicyBase
{
public:
  typedef MMPolicyOutOfProcess MMPolicyT;

  explicit MMPolicyOutOfProcess(HANDLE aProcess)
    : mProcess(nullptr)
    , mMapping(nullptr)
    , mLocalView(nullptr)
    , mRemoteView(nullptr)
    , mReservationSize(0)
    , mCommitOffset(0)
  {
    MOZ_ASSERT(aProcess);
    ::DuplicateHandle(::GetCurrentProcess(), aProcess, ::GetCurrentProcess(),
                      &mProcess, kAccessFlags, FALSE, 0);
    MOZ_ASSERT(mProcess);
  }

  explicit MMPolicyOutOfProcess(DWORD aPid)
    : mProcess(::OpenProcess(kAccessFlags, FALSE, aPid))
    , mMapping(nullptr)
    , mLocalView(nullptr)
    , mRemoteView(nullptr)
    , mReservationSize(0)
    , mCommitOffset(0)
  {
    MOZ_ASSERT(mProcess);
  }

  ~MMPolicyOutOfProcess()
  {
    Destroy();
  }

  MMPolicyOutOfProcess(MMPolicyOutOfProcess&& aOther)
    : mProcess(nullptr)
    , mMapping(nullptr)
    , mLocalView(nullptr)
    , mRemoteView(nullptr)
    , mReservationSize(0)
    , mCommitOffset(0)
  {
    *this = Move(aOther);
  }

  MMPolicyOutOfProcess(const MMPolicyOutOfProcess& aOther) = delete;
  MMPolicyOutOfProcess& operator=(const MMPolicyOutOfProcess&) = delete;

  MMPolicyOutOfProcess& operator=(MMPolicyOutOfProcess&& aOther)
  {
    Destroy();

    mProcess = aOther.mProcess;
    aOther.mProcess = nullptr;

    mMapping = aOther.mMapping;
    aOther.mMapping = nullptr;

    mLocalView = aOther.mLocalView;
    aOther.mLocalView = nullptr;

    mRemoteView = aOther.mRemoteView;
    aOther.mRemoteView = nullptr;

    mReservationSize = aOther.mReservationSize;
    aOther.mReservationSize = 0;

    mCommitOffset = aOther.mCommitOffset;
    aOther.mCommitOffset = 0;

    return *this;
  }

  explicit operator bool() const
  {
    return mProcess && mMapping && mLocalView && mRemoteView;
  }

  bool ShouldUnhookUponDestruction() const
  {
    // We don't clean up hooks for remote processes; they are expected to
    // outlive our process.
    return false;
  }

  bool Read(void* aToPtr, const void* aFromPtr, size_t aLen) const
  {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    SIZE_T numBytes = 0;
    BOOL ok = ::ReadProcessMemory(mProcess, aFromPtr, aToPtr, aLen, &numBytes);
    return ok && numBytes == aLen;
  }

  bool Write(void* aToPtr, const void* aFromPtr, size_t aLen) const
  {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    SIZE_T numBytes = 0;
    BOOL ok = ::WriteProcessMemory(mProcess, aToPtr, aFromPtr, aLen, &numBytes);
    return ok && numBytes == aLen;
  }

  bool Protect(void* aVAddress, size_t aSize, uint32_t aProtFlags,
               uint32_t* aPrevProtFlags) const
  {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    MOZ_ASSERT(aPrevProtFlags);
    BOOL ok = ::VirtualProtectEx(mProcess, aVAddress, aSize, aProtFlags,
                                 reinterpret_cast<PDWORD>(aPrevProtFlags));
    if (!ok && aPrevProtFlags) {
      // VirtualProtectEx can fail but still set valid protection flags.
      // Let's clear those upon failure.
      *aPrevProtFlags = 0;
    }

    return !!ok;
  }

  /**
   * @return true if the page that hosts aVAddress is accessible.
   */
  bool IsPageAccessible(void* aVAddress) const
  {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = ::VirtualQueryEx(mProcess, aVAddress, &mbi, sizeof(mbi));

    return result && mbi.AllocationProtect && (mbi.Type & MEM_IMAGE) &&
           mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS;
  }

  bool FlushInstructionCache() const
  {
    return !!::FlushInstructionCache(mProcess, nullptr, 0);
  }

protected:
  uint8_t* GetLocalView() const
  {
    return mLocalView;
  }

  uintptr_t GetRemoteView() const
  {
    return reinterpret_cast<uintptr_t>(mRemoteView);
  }

  /**
   * @return the effective number of bytes reserved, or 0 on failure
   */
  uint32_t Reserve(const uint32_t aSize)
  {
    if (!aSize || !mProcess) {
      return 0;
    }

    if (mRemoteView) {
      MOZ_ASSERT(mReservationSize >= aSize);
      return mReservationSize;
    }

    mReservationSize = ComputeAllocationSize(aSize);

    mMapping = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
                                   PAGE_EXECUTE_READWRITE | SEC_RESERVE,
                                   0, mReservationSize, nullptr);
    if (!mMapping) {
      return 0;
    }

    mLocalView = static_cast<uint8_t*>(
                   ::MapViewOfFile(mMapping, FILE_MAP_WRITE, 0, 0, 0));
    if (!mLocalView) {
      return 0;
    }

    mRemoteView = MapRemoteViewOfFile(mMapping, mProcess, 0ULL,
                                      nullptr, 0, 0, PAGE_EXECUTE_READ);
    if (!mRemoteView) {
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

    PVOID local = ::VirtualAlloc(mLocalView + mCommitOffset, GetPageSize(),
                                 MEM_COMMIT, PAGE_READWRITE);
    if (!local) {
      return false;
    }

    PVOID remote = ::VirtualAllocEx(mProcess,
                                    static_cast<uint8_t*>(mRemoteView) +
                                      mCommitOffset, GetPageSize(),
                                    MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!remote) {
      return false;
    }

    mCommitOffset += GetPageSize();
    return true;
  }

private:
  void Destroy()
  {
    // We always leak the remote view
    if (mLocalView) {
      ::UnmapViewOfFile(mLocalView);
      mLocalView = nullptr;
    }

    if (mMapping) {
      ::CloseHandle(mMapping);
      mMapping = nullptr;
    }

    if (mProcess) {
      ::CloseHandle(mProcess);
      mProcess = nullptr;
    }
  }

private:
  HANDLE    mProcess;
  HANDLE    mMapping;
  uint8_t*  mLocalView;
  PVOID     mRemoteView;
  uint32_t  mReservationSize;
  uint32_t  mCommitOffset;

  static const DWORD kAccessFlags = PROCESS_VM_OPERATION | PROCESS_VM_READ |
                                    PROCESS_VM_WRITE;
};

} // namespace interceptor
} // namespace mozilla

#endif // mozilla_interceptor_MMPolicies_h

