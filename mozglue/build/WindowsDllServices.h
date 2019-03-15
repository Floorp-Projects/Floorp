/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_WindowsDllServices_h
#define mozilla_glue_WindowsDllServices_h

#include "mozilla/Assertions.h"
#include "mozilla/Authenticode.h"
#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsDllBlocklist.h"

#if defined(MOZILLA_INTERNAL_API)

#  include "MainThreadUtils.h"
#  include "mozilla/SystemGroup.h"
#  include "nsISupportsImpl.h"
#  include "nsString.h"
#  include "nsThreadUtils.h"

#endif  // defined(MOZILLA_INTERNAL_API)

// For PCUNICODE_STRING
#include <winternl.h>

namespace mozilla {
namespace glue {

// Holds data about a top-level DLL load event, for the purposes of later
// evaluating the DLLs for trustworthiness. DLLs are loaded recursively,
// so we hold the top-level DLL and all child DLLs in mModules.
class ModuleLoadEvent {
 public:
  class ModuleInfo {
   public:
    ModuleInfo() = default;
    ~ModuleInfo() = default;
    ModuleInfo(const ModuleInfo& aOther) = delete;
    ModuleInfo(ModuleInfo&& aOther) = default;

    ModuleInfo operator=(const ModuleInfo& aOther) = delete;
    ModuleInfo operator=(ModuleInfo&& aOther) = delete;

    uintptr_t mBase;
    UniquePtr<wchar_t[]> mLdrName;
    UniquePtr<wchar_t[]> mFullPath;
    double mLoadDurationMS;
  };

  ModuleLoadEvent() = default;
  ~ModuleLoadEvent() = default;
  ModuleLoadEvent(const ModuleLoadEvent& aOther) = delete;
  ModuleLoadEvent(ModuleLoadEvent&& aOther) = default;

  ModuleLoadEvent& operator=(const ModuleLoadEvent& aOther) = delete;
  ModuleLoadEvent& operator=(ModuleLoadEvent&& aOther) = delete;

  DWORD mThreadID;
  uint64_t mProcessUptimeMS;
  Vector<ModuleInfo, 0, InfallibleAllocPolicy> mModules;

  // Stores instruction pointers, top-to-bottom.
  Vector<uintptr_t, 0, InfallibleAllocPolicy> mStack;
};

namespace detail {

class DllServicesBase : public Authenticode {
 public:
  /**
   * WARNING: This method is called from within an unsafe context that holds
   *          multiple locks inside the Windows loader. The only thing that
   *          this function should be used for is dispatching the event to our
   *          event loop so that it may be handled in a safe context.
   */
  virtual void DispatchDllLoadNotification(PCUNICODE_STRING aDllName) = 0;

  /**
   * This function accepts module load events to be processed later for
   * the untrusted modules telemetry ping.
   *
   * WARNING: This method is run from within the Windows loader and should
   *          only perform trivial, loader-friendly, operations.
   */
  virtual void NotifyUntrustedModuleLoads(
      const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>&
          aEvents) = 0;

  void SetAuthenticodeImpl(Authenticode* aAuthenticode) {
    mAuthenticode = aAuthenticode;
  }

  // In debug builds, we override GetBinaryOrgName to add a Gecko-specific
  // assertion. OTOH, we normally do not want people overriding this function,
  // so we'll make it final in the release case, thus covering all bases.
#if defined(DEBUG)
  UniquePtr<wchar_t[]> GetBinaryOrgName(const wchar_t* aFilePath) override
#else
  UniquePtr<wchar_t[]> GetBinaryOrgName(const wchar_t* aFilePath) final
#endif  // defined(DEBUG)
  {
    if (!mAuthenticode) {
      return nullptr;
    }

    return mAuthenticode->GetBinaryOrgName(aFilePath);
  }

  void DisableFull() { DllBlocklist_SetFullDllServices(nullptr); }

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
};

}  // namespace detail

#if defined(MOZILLA_INTERNAL_API)

class DllServices : public detail::DllServicesBase {
 public:
  void DispatchDllLoadNotification(PCUNICODE_STRING aDllName) final {
    nsDependentSubstring strDllName(aDllName->Buffer,
                                    aDllName->Length / sizeof(wchar_t));

    nsCOMPtr<nsIRunnable> runnable(NewRunnableMethod<bool, nsString>(
        "DllServices::NotifyDllLoad", this, &DllServices::NotifyDllLoad,
        NS_IsMainThread(), strDllName));

    SystemGroup::Dispatch(TaskCategory::Other, runnable.forget());
  }

#  if defined(DEBUG)
  UniquePtr<wchar_t[]> GetBinaryOrgName(const wchar_t* aFilePath) final {
    // This function may perform disk I/O, so we should never call it on the
    // main thread.
    MOZ_ASSERT(!NS_IsMainThread());
    return detail::DllServicesBase::GetBinaryOrgName(aFilePath);
  }
#  endif  // defined(DEBUG)

  NS_INLINE_DECL_THREADSAFE_VIRTUAL_REFCOUNTING(DllServices)

 protected:
  DllServices() = default;
  ~DllServices() = default;

  virtual void NotifyDllLoad(const bool aIsMainThread,
                             const nsString& aDllName) = 0;
};

#else

class BasicDllServices final : public detail::DllServicesBase {
 public:
  BasicDllServices() { EnableBasic(); }

  ~BasicDllServices() = default;

  // Not useful in this class, so provide a default implementation
  virtual void DispatchDllLoadNotification(PCUNICODE_STRING aDllName) override {
  }

  virtual void NotifyUntrustedModuleLoads(
      const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents)
      override {}
};

#endif  // defined(MOZILLA_INTERNAL_API)

}  // namespace glue
}  // namespace mozilla

#endif  // mozilla_glue_WindowsDllServices_h
