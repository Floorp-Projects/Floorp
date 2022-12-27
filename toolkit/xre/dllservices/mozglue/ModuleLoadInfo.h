/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ModuleLoadInfo_h
#define mozilla_ModuleLoadInfo_h

#include "mozilla/NativeNt.h"
#include "mozilla/Vector.h"
#include "mozilla/Unused.h"

namespace mozilla {

struct ModuleLoadInfo final {
  // If you add a new value or change the meaning of the values, please
  // update createLoadStatusElement in aboutSupport.js accordingly, which
  // defines text labels of these enum values displayed on about:support.
  enum class Status : uint32_t {
    Loaded = 0,
    Blocked,
    Redirected,
  };

  // We do not provide these methods inside Gecko proper.
#if !defined(MOZILLA_INTERNAL_API)

  /**
   * This constructor is for use by the LdrLoadDll hook.
   */
  explicit ModuleLoadInfo(PCUNICODE_STRING aRequestedDllName)
      : mLoadTimeInfo(),
        mThreadId(nt::RtlGetCurrentThreadId()),
        mRequestedDllName(aRequestedDllName),
        mBaseAddr(nullptr),
        mStatus(Status::Loaded),
        mIsDependent(false) {
#  if defined(IMPL_MFBT)
    ::QueryPerformanceCounter(&mBeginTimestamp);
#  else
    ::RtlQueryPerformanceCounter(&mBeginTimestamp);
#  endif  // defined(IMPL_MFBT)
  }

  /**
   * This constructor is used by the NtMapViewOfSection hook IF AND ONLY IF
   * the LdrLoadDll hook did not already construct a ModuleLoadInfo for the
   * current DLL load. This may occur while the loader is loading dependencies
   * of another library.
   */
  ModuleLoadInfo(nt::AllocatedUnicodeString&& aSectionName,
                 const void* aBaseAddr, Status aLoadStatus, bool aIsDependent)
      : mLoadTimeInfo(),
        mThreadId(nt::RtlGetCurrentThreadId()),
        mSectionName(std::move(aSectionName)),
        mBaseAddr(aBaseAddr),
        mStatus(aLoadStatus),
        mIsDependent(aIsDependent) {
#  if defined(IMPL_MFBT)
    ::QueryPerformanceCounter(&mBeginTimestamp);
#  else
    ::RtlQueryPerformanceCounter(&mBeginTimestamp);
#  endif  // defined(IMPL_MFBT)
  }

  /**
   * Marks the time that LdrLoadDll began loading this library.
   */
  void SetBeginLoadTimeStamp() {
#  if defined(IMPL_MFBT)
    ::QueryPerformanceCounter(&mLoadTimeInfo);
#  else
    ::RtlQueryPerformanceCounter(&mLoadTimeInfo);
#  endif  // defined(IMPL_MFBT)
  }

  /**
   * Marks the time that LdrLoadDll finished loading this library.
   */
  void SetEndLoadTimeStamp() {
    LARGE_INTEGER endTimeStamp;
#  if defined(IMPL_MFBT)
    ::QueryPerformanceCounter(&endTimeStamp);
#  else
    ::RtlQueryPerformanceCounter(&endTimeStamp);
#  endif  // defined(IMPL_MFBT)

    LONGLONG& timeInfo = mLoadTimeInfo.QuadPart;
    if (!timeInfo) {
      return;
    }

    timeInfo = endTimeStamp.QuadPart - timeInfo;
  }

  /**
   * Saves the current thread's call stack.
   */
  void CaptureBacktrace() {
    const DWORD kMaxBacktraceSize = 512;

    if (!mBacktrace.resize(kMaxBacktraceSize)) {
      return;
    }

    // We don't use a Win32 variant here because Win32's CaptureStackBackTrace
    // is just a macro that resolve to this function anyway.
    WORD numCaptured = ::RtlCaptureStackBackTrace(2, kMaxBacktraceSize,
                                                  mBacktrace.begin(), nullptr);
    Unused << mBacktrace.resize(numCaptured);
    // These backtraces might stick around for a while, so let's trim any
    // excess memory.
    mBacktrace.shrinkStorageToFit();
  }

#endif  // !defined(MOZILLA_INTERNAL_API)

  ModuleLoadInfo(ModuleLoadInfo&&) = default;
  ModuleLoadInfo& operator=(ModuleLoadInfo&&) = default;

  ModuleLoadInfo() = delete;
  ModuleLoadInfo(const ModuleLoadInfo&) = delete;
  ModuleLoadInfo& operator=(const ModuleLoadInfo&) = delete;

  /**
   * A "bare" module load is one that was mapped without the code passing
   * through a call to ntdll!LdrLoadDll.
   */
  bool IsBare() const {
    // SetBeginLoadTimeStamp() and SetEndLoadTimeStamp() are only called by the
    // LdrLoadDll hook, so when mLoadTimeInfo == 0, we know that we are bare.
    return !mLoadTimeInfo.QuadPart;
  }

  /**
   * Returns true for DLL loads where LdrLoadDll was called but
   * NtMapViewOfSection was not. This will happen for DLL requests where the DLL
   * was already mapped into memory by a previous request.
   */
  bool WasMapped() const { return !mSectionName.IsEmpty(); }

  /**
   * Returns true for DLL load which was denied by our blocklist.
   */
  bool WasDenied() const {
    return mStatus == ModuleLoadInfo::Status::Blocked ||
           mStatus == ModuleLoadInfo::Status::Redirected;
  }

  // Timestamp for the creation of this event
  LARGE_INTEGER mBeginTimestamp;
  // Duration of the LdrLoadDll call
  LARGE_INTEGER mLoadTimeInfo;
  // Thread ID of this DLL load
  DWORD mThreadId;
  // The name requested of LdrLoadDll by its caller
  nt::AllocatedUnicodeString mRequestedDllName;
  // The name of the DLL that backs section that was mapped by the loader. This
  // string is the effective name of the DLL that was resolved by the loader's
  // path search algorithm.
  nt::AllocatedUnicodeString mSectionName;
  // The base address of the module's mapped section
  const void* mBaseAddr;
  // If the module was successfully loaded, stack trace of the DLL load request
  Vector<PVOID, 0, nt::RtlAllocPolicy> mBacktrace;
  // The status of DLL load
  Status mStatus;
  // Whether the module is one of the executables's dependent modules or not
  bool mIsDependent;
};

using ModuleLoadInfoVec = Vector<ModuleLoadInfo, 0, nt::RtlAllocPolicy>;

}  // namespace mozilla

#endif  // mozilla_ModuleLoadInfo_h
