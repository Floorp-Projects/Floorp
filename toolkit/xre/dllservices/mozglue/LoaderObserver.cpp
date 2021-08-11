/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoaderObserver.h"

#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/glue/WindowsUnicode.h"
#include "mozilla/StackWalk_windows.h"
#include "mozilla/TimeStamp.h"

namespace {

struct LoadContext {
  LoadContext(mozilla::ProfilerLabel&& aLabel,
              mozilla::UniquePtr<char[]>&& aDynamicStringStorage)
      : mProfilerLabel(std::move(aLabel)),
        mDynamicStringStorage(std::move(aDynamicStringStorage)),
        mStartTime(mozilla::TimeStamp::Now()) {}
  mozilla::ProfilerLabel mProfilerLabel;
  mozilla::UniquePtr<char[]> mDynamicStringStorage;
  mozilla::TimeStamp mStartTime;
};

}  // anonymous namespace

namespace mozilla {

extern glue::Win32SRWLock gDllServicesLock;
extern glue::detail::DllServicesBase* gDllServices;

namespace glue {

void LoaderObserver::OnBeginDllLoad(void** aContext,
                                    PCUNICODE_STRING aRequestedDllName) {
  MOZ_ASSERT(aContext);
  if (IsProfilerPresent()) {
    UniquePtr<char[]> utf8RequestedDllName(WideToUTF8(aRequestedDllName));
    const char* dynamicString = utf8RequestedDllName.get();
    *aContext = new LoadContext(
        ProfilerLabelBegin("mozilla::glue::LoaderObserver::OnBeginDllLoad",
                           dynamicString, &aContext),
        std::move(utf8RequestedDllName));
  }

#ifdef _M_AMD64
  // Prevent the stack walker from suspending this thread when LdrLoadDll
  // holds the RtlLookupFunctionEntry lock.
  SuppressStackWalking();
#endif
}

bool LoaderObserver::SubstituteForLSP(PCUNICODE_STRING aLSPLeafName,
                                      PHANDLE aOutHandle) {
  // Currently unsupported
  return false;
}

void LoaderObserver::OnEndDllLoad(void* aContext, NTSTATUS aNtStatus,
                                  ModuleLoadInfo&& aModuleLoadInfo) {
#ifdef _M_AMD64
  DesuppressStackWalking();
#endif

  UniquePtr<LoadContext> loadContext(static_cast<LoadContext*>(aContext));
  if (loadContext && IsValidProfilerLabel(loadContext->mProfilerLabel)) {
    ProfilerLabelEnd(loadContext->mProfilerLabel);
    BASE_PROFILER_MARKER_TEXT(
        "DllLoad", OTHER,
        MarkerTiming::IntervalUntilNowFrom(loadContext->mStartTime),
        mozilla::ProfilerString8View::WrapNullTerminatedString(
            loadContext->mDynamicStringStorage.get()));
  }

  // We want to record a denied DLL load regardless of |aNtStatus| because
  // |aNtStatus| is set to access-denied when DLL load was blocked.
  if ((!NT_SUCCESS(aNtStatus) && !aModuleLoadInfo.WasDenied()) ||
      !aModuleLoadInfo.WasMapped()) {
    return;
  }

  {  // Scope for lock
    AutoSharedLock lock(gDllServicesLock);
    if (gDllServices) {
      gDllServices->DispatchDllLoadNotification(std::move(aModuleLoadInfo));
      return;
    }
  }

  // No dll services, save for later
  AutoExclusiveLock lock(mLock);
  if (!mEnabled) {
    return;
  }

  if (!mModuleLoads) {
    mModuleLoads = new ModuleLoadInfoVec();
  }

  Unused << mModuleLoads->emplaceBack(
      std::forward<ModuleLoadInfo>(aModuleLoadInfo));
}

void LoaderObserver::Forward(nt::LoaderObserver* aNext) {
  MOZ_ASSERT_UNREACHABLE(
      "This implementation does not forward to any more "
      "nt::LoaderObserver objects");
}

void LoaderObserver::Forward(detail::DllServicesBase* aNext) {
  MOZ_ASSERT(aNext);
  if (!aNext) {
    return;
  }

  ModuleLoadInfoVec* moduleLoads = nullptr;

  {  // Scope for lock
    AutoExclusiveLock lock(mLock);
    moduleLoads = mModuleLoads;
    mModuleLoads = nullptr;
  }

  if (!moduleLoads) {
    return;
  }

  aNext->DispatchModuleLoadBacklogNotification(std::move(*moduleLoads));
  delete moduleLoads;
}

void LoaderObserver::Disable() {
  ModuleLoadInfoVec* moduleLoads = nullptr;

  {  // Scope for lock
    AutoExclusiveLock lock(mLock);
    moduleLoads = mModuleLoads;
    mModuleLoads = nullptr;
    mEnabled = false;
  }

  delete moduleLoads;
}

void LoaderObserver::OnForward(ModuleLoadInfoVec&& aInfo) {
  AutoExclusiveLock lock(mLock);
  if (!mModuleLoads) {
    mModuleLoads = new ModuleLoadInfoVec();
  }

  MOZ_ASSERT(mModuleLoads->empty());
  if (mModuleLoads->empty()) {
    *mModuleLoads = std::move(aInfo);
  } else {
    // This should not happen, but we can handle it
    for (auto&& item : aInfo) {
      Unused << mModuleLoads->append(std::move(item));
    }
  }
}

}  // namespace glue
}  // namespace mozilla
