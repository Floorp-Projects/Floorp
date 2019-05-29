/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedDllsHandler.h"

#include <windows.h>

#include "mozilla/Atomics.h"
#include "mozilla/mozalloc.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "nsWindowsHelpers.h"  // For AutoCriticalSection

namespace mozilla {
namespace glue {

// Copies a null-terminated string. Upon error, returns nullptr.
static UniquePtr<wchar_t[]> CopyString(const UniquePtr<wchar_t[]>& aOther) {
  if (!aOther) {
    return nullptr;
  }
  size_t chars = wcslen(aOther.get());
  auto ret = MakeUnique<wchar_t[]>(chars + 1);
  if (wcsncpy_s(ret.get(), chars + 1, aOther.get(), chars)) {
    return nullptr;
  }
  return ret;
}

// Creates a UniquePtr<wchar_t[]> from a PCUNICODE_STRING string.
// Upon error, returns nullptr.
static UniquePtr<wchar_t[]> CopyString(PCUNICODE_STRING aOther) {
  if (!aOther || !aOther->Buffer) {
    return nullptr;
  }
  size_t chars = aOther->Length / sizeof(wchar_t);
  auto ret = MakeUnique<wchar_t[]>(chars + 1);
  if (wcsncpy_s(ret.get(), chars + 1, aOther->Buffer, chars)) {
    return nullptr;
  }
  return ret;
}

// Basic wrapper around ::GetModuleFileNameW.
// Returns the full path of the loaded module specified by aModuleBase.
// Upon error, returns nullptr.
static UniquePtr<wchar_t[]> GetModuleFullPath(uintptr_t aModuleBase) {
  size_t allocated = MAX_PATH;
  auto ret = MakeUnique<wchar_t[]>(allocated);
  size_t len;
  while (true) {
    len = (size_t)::GetModuleFileNameW((HMODULE)aModuleBase, ret.get(),
                                       allocated);
    if (!len) {
      return nullptr;
    }
    if (len == allocated && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      allocated *= 2;
      ret = MakeUnique<wchar_t[]>(allocated);
      continue;
    }
    break;
  }
  // The buffer may much bigger than needed. Return an efficiently-allocated
  // buffer.
  return CopyString(ret);
}

// To track call depth and recursively-loaded modules, we must store this data
// in thread local storage.
class TLSData {
 public:
  Vector<ModuleLoadEvent::ModuleInfo, 0, InfallibleAllocPolicy> mModulesLoaded;
  int mCallDepth = 0;
};

static MOZ_THREAD_LOCAL(TLSData*) sTlsData;

// This singleton class does the underlying work for UntrustedDllsHandler
class UntrustedDllsHandlerImpl {
  // Refcounting gives us a way to synchronize call lifetime vs object lifetime.
  // We don't have access to NS_INLINE_DECL_THREADSAFE_REFCOUNTING from mozglue,
  // but it's easy to roll our own.
  Atomic<int32_t> mRefCnt;

  // In order to prevent sInstance from being "woken back up" after it's been
  // cleared on shutdown, this will let us know if sInstance is empty because
  // it's not initialized yet, or because it's been cleared on shutdown.
  static Atomic<bool> sInstanceHasBeenSet;

  // Singleton reference
  static StaticRefPtr<UntrustedDllsHandlerImpl> sInstance;

  // Holds a list of module load events. This gets emptied upon calling
  // UntrustedDllsHandler::TakePendingEvents
  Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> mModuleLoadEvents;

  // Holds a list of module full paths that we've already handled, so we can
  // skip duplicates.
  Vector<mozilla::UniquePtr<wchar_t[]>> mModuleHistory;

  // This lock protects gModuleLoadEvents and gModuleHistory.
  //
  // You must only make trivial, loader-lock-friendly calls within the lock. We
  // cannot risk re-entering the loader at this point.
  CRITICAL_SECTION mDataLock;

  UntrustedDllsHandlerImpl() { InitializeCriticalSection(&mDataLock); }

  ~UntrustedDllsHandlerImpl() {
    {  // Scope for lock
      // Ensure pending ops are complete.
      AutoCriticalSection lock(&mDataLock);
    }
    DeleteCriticalSection(&mDataLock);
  }

 public:
  static RefPtr<UntrustedDllsHandlerImpl> GetInstance() {
    if (sInstanceHasBeenSet) {
      return sInstance;
    }

    sInstance = new UntrustedDllsHandlerImpl();
    sInstanceHasBeenSet = true;
    return sInstance;
  }

  static void Shutdown() { sInstance = nullptr; }

  int32_t AddRef() { return ++mRefCnt; }

  int32_t Release() {
    int32_t ret = --mRefCnt;
    if (!ret) {
      delete this;
    }
    return ret;
  }

  // Called after a successful module load at the top level. Now we are safe
  // to package up the event and save for later processing.
  void OnAfterTopLevelModuleLoad() {
    // Hold a reference to ensure we don't get deleted during this call.
    RefPtr<UntrustedDllsHandlerImpl> refHolder(this);
    if (!refHolder) {
      return;
    }

    ModuleLoadEvent thisEvent;

    TLSData* tlsData = sTlsData.get();
    if (!tlsData) {
      return;
    }

    {  // Scope for lock
      // Lock around gModuleHistory to prune out modules we've handled before.
      // Only trivial calls allowed during lock (don't invoke the loader)
      AutoCriticalSection lock(&mDataLock);

      // There will rarely be more than a couple items in
      // tlsData->mModulesLoaded, so this is efficient enough.
      for (auto& module : tlsData->mModulesLoaded) {
        if (module.mFullPath && wcslen(module.mFullPath.get())) {
          bool foundInHistory = false;
          for (auto& h : mModuleHistory) {
            if (!wcsicmp(h.get(), module.mFullPath.get())) {
              foundInHistory = true;
              break;
            }
          }
          if (foundInHistory) {
            continue;
          }
          Unused << mModuleHistory.append(CopyString(module.mFullPath));
        }
        Unused << thisEvent.mModules.emplaceBack(std::move(module));
      }
    }

    tlsData->mModulesLoaded.clear();

    if (thisEvent.mModules.empty()) {
      return;  // All modules have been filtered out; nothing further to do.
    }

    thisEvent.mThreadID = GetCurrentThreadId();

    TimeStamp processCreation = TimeStamp::ProcessCreation();
    TimeDuration td = TimeStamp::Now() - processCreation;
    thisEvent.mProcessUptimeMS = (uint64_t)td.ToMilliseconds();

    static const uint32_t kMaxFrames = 500;
    auto frames = MakeUnique<void*[]>(kMaxFrames);

    // Setting FramesToSkip to 1 so the caller will see itself as the top frame.
    USHORT frameCount =
        CaptureStackBackTrace(1, kMaxFrames, frames.get(), nullptr);
    if (thisEvent.mStack.reserve(frameCount)) {
      for (size_t i = 0; i < frameCount; ++i) {
        Unused << thisEvent.mStack.append((uintptr_t)frames[i]);
      }
    }

    // Lock in order to store the new event.
    // Only trivial calls allowed during lock (don't invoke the loader)
    AutoCriticalSection lock(&mDataLock);
    Unused << mModuleLoadEvents.emplaceBack(std::move(thisEvent));
  }

  // Returns true if aOut is no longer empty.
  bool TakePendingEvents(
      Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy>& aOut) {
    // Hold a reference to ensure we don't get deleted during this call.
    RefPtr<UntrustedDllsHandlerImpl> refHolder(this);
    if (!refHolder) {
      return false;
    }

    AutoCriticalSection lock(&mDataLock);
    mModuleLoadEvents.swap(aOut);
    return !aOut.empty();
  }
};

Atomic<bool> UntrustedDllsHandlerImpl::sInstanceHasBeenSet;
StaticRefPtr<UntrustedDllsHandlerImpl> UntrustedDllsHandlerImpl::sInstance;

/* static */
void UntrustedDllsHandler::Init() { Unused << sTlsData.init(); }

#ifdef DEBUG
/* static */
void UntrustedDllsHandler::Shutdown() { UntrustedDllsHandlerImpl::Shutdown(); }
#endif  // DEBUG

/* static */
void UntrustedDllsHandler::EnterLoaderCall() {
  if (!sTlsData.initialized()) {
    return;
  }
  if (!sTlsData.get()) {
    sTlsData.set(new TLSData());
  }
  sTlsData.get()->mCallDepth++;
}

/* static */
void UntrustedDllsHandler::ExitLoaderCall() {
  if (!sTlsData.initialized()) {
    return;
  }
  if (!--(sTlsData.get()->mCallDepth)) {
    delete sTlsData.get();
    sTlsData.set(nullptr);
  }
}

/* static */
void UntrustedDllsHandler::OnAfterModuleLoad(uintptr_t aBaseAddr,
                                             PUNICODE_STRING aLdrModuleName,
                                             double aLoadDurationMS) {
  RefPtr<UntrustedDllsHandlerImpl> p(UntrustedDllsHandlerImpl::GetInstance());
  if (!p) {
    return;
  }

  if (!sTlsData.initialized()) {
    return;
  }
  TLSData* tlsData = sTlsData.get();
  if (!tlsData) {
    return;
  }

  ModuleLoadEvent::ModuleInfo moduleInfo;
  moduleInfo.mLdrName = CopyString(aLdrModuleName);
  moduleInfo.mBase = aBaseAddr;
  moduleInfo.mFullPath = GetModuleFullPath(aBaseAddr);
  moduleInfo.mLoadDurationMS = aLoadDurationMS;

  Unused << tlsData->mModulesLoaded.emplaceBack(std::move(moduleInfo));

  if (tlsData->mCallDepth > 1) {
    // Recursive call; bail and wait until top-level call can proceed.
    return;
  }

  p->OnAfterTopLevelModuleLoad();
}

/* static */
bool UntrustedDllsHandler::TakePendingEvents(
    Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy>& aOut) {
  RefPtr<UntrustedDllsHandlerImpl> p(UntrustedDllsHandlerImpl::GetInstance());
  if (!p) {
    return false;
  }
  return p->TakePendingEvents(aOut);
}

}  // namespace glue
}  // namespace mozilla
