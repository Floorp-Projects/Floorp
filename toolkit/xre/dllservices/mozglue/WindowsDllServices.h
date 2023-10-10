/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_WindowsDllServices_h
#define mozilla_glue_WindowsDllServices_h

#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/Authenticode.h"
#include "mozilla/LoaderAPIInterfaces.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/WindowsDllBlocklist.h"
#include "mozilla/mozalloc.h"

#if defined(MOZILLA_INTERNAL_API)
#  include "MainThreadUtils.h"
#  include "nsISupportsImpl.h"
#  include "nsString.h"
#  include "nsThreadUtils.h"
#  include "prthread.h"
#  include "mozilla/SchedulerGroup.h"
#endif  // defined(MOZILLA_INTERNAL_API)

// For PCUNICODE_STRING
#include <winternl.h>

namespace mozilla {
namespace glue {
namespace detail {

class DllServicesBase : public Authenticode {
 public:
  /**
   * WARNING: This method is called from within an unsafe context that holds
   *          multiple locks inside the Windows loader. The only thing that
   *          this function should be used for is dispatching the event to our
   *          event loop so that it may be handled in a safe context.
   */
  virtual void DispatchDllLoadNotification(ModuleLoadInfo&& aModLoadInfo) = 0;

  /**
   * This function accepts module load events to be processed later for
   * the untrusted modules telemetry ping.
   *
   * WARNING: This method is run from within the Windows loader and should
   *          only perform trivial, loader-friendly operations.
   */
  virtual void DispatchModuleLoadBacklogNotification(
      ModuleLoadInfoVec&& aEvents) = 0;

  void SetAuthenticodeImpl(Authenticode* aAuthenticode) {
    mAuthenticode = aAuthenticode;
  }

  void SetWinLauncherServices(const nt::WinLauncherServices& aWinLauncher) {
    mWinLauncher = aWinLauncher;
  }

  template <typename... Args>
  LauncherVoidResultWithLineInfo InitDllBlocklistOOP(Args&&... aArgs) {
    MOZ_RELEASE_ASSERT(mWinLauncher.mInitDllBlocklistOOP);
    return mWinLauncher.mInitDllBlocklistOOP(std::forward<Args>(aArgs)...);
  }

  template <typename... Args>
  void HandleLauncherError(Args&&... aArgs) {
    MOZ_RELEASE_ASSERT(mWinLauncher.mHandleLauncherError);
    mWinLauncher.mHandleLauncherError(std::forward<Args>(aArgs)...);
  }

  nt::SharedSection* GetSharedSection() { return mWinLauncher.mSharedSection; }

  // In debug builds we override GetBinaryOrgName to add a Gecko-specific
  // assertion. OTOH, we normally do not want people overriding this function,
  // so we'll make it final in the release case, thus covering all bases.
#if defined(DEBUG)
  UniquePtr<wchar_t[]> GetBinaryOrgName(
      const wchar_t* aFilePath, bool* aHasNestedMicrosoftSignature = nullptr,
      AuthenticodeFlags aFlags = AuthenticodeFlags::Default) override
#else
  UniquePtr<wchar_t[]> GetBinaryOrgName(
      const wchar_t* aFilePath, bool* aHasNestedMicrosoftSignature = nullptr,
      AuthenticodeFlags aFlags = AuthenticodeFlags::Default) final
#endif  // defined(DEBUG)
  {
    if (!mAuthenticode) {
      return nullptr;
    }

    return mAuthenticode->GetBinaryOrgName(
        aFilePath, aHasNestedMicrosoftSignature, aFlags);
  }

  virtual void DisableFull() { DllBlocklist_SetFullDllServices(nullptr); }

  DllServicesBase(const DllServicesBase&) = delete;
  DllServicesBase(DllServicesBase&&) = delete;
  DllServicesBase& operator=(const DllServicesBase&) = delete;
  DllServicesBase& operator=(DllServicesBase&&) = delete;

 protected:
  DllServicesBase() : mAuthenticode(nullptr) {}

  virtual ~DllServicesBase() = default;

  void EnableFull() { DllBlocklist_SetFullDllServices(this); }
  void EnableBasic() { DllBlocklist_SetBasicDllServices(this); }

 private:
  Authenticode* mAuthenticode;
  nt::WinLauncherServices mWinLauncher;
};

}  // namespace detail

#if defined(MOZILLA_INTERNAL_API)

struct EnhancedModuleLoadInfo final {
  explicit EnhancedModuleLoadInfo(ModuleLoadInfo&& aModLoadInfo)
      : mNtLoadInfo(std::move(aModLoadInfo)) {
    // Only populate mThreadName when we're on the same thread as the event
    if (mNtLoadInfo.mThreadId == ::GetCurrentThreadId()) {
      mThreadName = PR_GetThreadName(PR_GetCurrentThread());
    }
    MOZ_ASSERT(!mNtLoadInfo.mSectionName.IsEmpty());
  }

  EnhancedModuleLoadInfo(EnhancedModuleLoadInfo&&) = default;
  EnhancedModuleLoadInfo& operator=(EnhancedModuleLoadInfo&&) = default;

  EnhancedModuleLoadInfo(const EnhancedModuleLoadInfo&) = delete;
  EnhancedModuleLoadInfo& operator=(const EnhancedModuleLoadInfo&) = delete;

  nsDependentString GetSectionName() const {
    return mNtLoadInfo.mSectionName.AsString();
  }

  using BacktraceType = decltype(ModuleLoadInfo::mBacktrace);

  ModuleLoadInfo mNtLoadInfo;
  nsCString mThreadName;
};

class DllServices : public detail::DllServicesBase {
 public:
  void DispatchDllLoadNotification(ModuleLoadInfo&& aModLoadInfo) final {
    // We only notify one blocked DLL load event per blocked DLL for the main
    // thread, because dispatching a notification can trigger a new blocked
    // DLL load if the DLL is registered as a WH_GETMESSAGE hook. In that case,
    // dispatching a notification with every load results in an infinite cycle,
    // see bug 1823412.
    if (aModLoadInfo.WasBlocked() && NS_IsMainThread()) {
      nsDependentString sectionName(aModLoadInfo.mSectionName.AsString());

      for (const auto& blockedModule : mMainThreadBlockedModules) {
        if (sectionName == blockedModule) {
          return;
        }
      }

      MOZ_ALWAYS_TRUE(mMainThreadBlockedModules.append(sectionName));
    }

    nsCOMPtr<nsIRunnable> runnable(
        NewRunnableMethod<StoreCopyPassByRRef<EnhancedModuleLoadInfo>>(
            "DllServices::NotifyDllLoad", this, &DllServices::NotifyDllLoad,
            std::move(aModLoadInfo)));
    SchedulerGroup::Dispatch(runnable.forget());
  }

  void DispatchModuleLoadBacklogNotification(
      ModuleLoadInfoVec&& aEvents) final {
    nsCOMPtr<nsIRunnable> runnable(
        NewRunnableMethod<StoreCopyPassByRRef<ModuleLoadInfoVec>>(
            "DllServices::NotifyModuleLoadBacklog", this,
            &DllServices::NotifyModuleLoadBacklog, std::move(aEvents)));

    SchedulerGroup::Dispatch(runnable.forget());
  }

#  if defined(DEBUG)
  UniquePtr<wchar_t[]> GetBinaryOrgName(
      const wchar_t* aFilePath, bool* aHasNestedMicrosoftSignature = nullptr,
      AuthenticodeFlags aFlags = AuthenticodeFlags::Default) final {
    // This function may perform disk I/O, so we should never call it on the
    // main thread.
    MOZ_ASSERT(!NS_IsMainThread());
    return detail::DllServicesBase::GetBinaryOrgName(
        aFilePath, aHasNestedMicrosoftSignature, aFlags);
  }
#  endif  // defined(DEBUG)

  NS_INLINE_DECL_THREADSAFE_VIRTUAL_REFCOUNTING(DllServices)

 protected:
  DllServices() = default;
  ~DllServices() = default;

  virtual void NotifyDllLoad(EnhancedModuleLoadInfo&& aModLoadInfo) = 0;
  virtual void NotifyModuleLoadBacklog(ModuleLoadInfoVec&& aEvents) = 0;

 private:
  // This vector has no associated lock. It must only be used on the main
  // thread.
  Vector<nsString> mMainThreadBlockedModules;
};

#else

class BasicDllServices final : public detail::DllServicesBase {
 public:
  BasicDllServices() { EnableBasic(); }

  ~BasicDllServices() = default;

  // Not useful in this class, so provide a default implementation
  virtual void DispatchDllLoadNotification(
      ModuleLoadInfo&& aModLoadInfo) override {}

  virtual void DispatchModuleLoadBacklogNotification(
      ModuleLoadInfoVec&& aEvents) override {}
};

#endif  // defined(MOZILLA_INTERNAL_API)

}  // namespace glue
}  // namespace mozilla

#endif  // mozilla_glue_WindowsDllServices_h
