/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoaderAPIInterfaces_h
#define mozilla_LoaderAPIInterfaces_h

#include "nscore.h"
#include "mozilla/ModuleLoadInfo.h"

namespace mozilla {
namespace nt {

class NS_NO_VTABLE LoaderObserver {
 public:
  /**
   * Notification that a DLL load has begun.
   *
   * @param aContext Outparam that allows this observer to store any context
   *                 information pertaining to the current load.
   * @param aRequestedDllName The DLL name requested by whatever invoked the
   *                          loader. This name may not match the effective
   *                          name of the DLL once the loader has completed
   *                          its path search.
   */
  virtual void OnBeginDllLoad(void** aContext,
                              PCUNICODE_STRING aRequestedDllName) = 0;

  /**
   * Query the observer to determine whether the DLL named |aLSPLeafName| needs
   * to be substituted with another module, and substitute the module handle
   * when necessary.
   *
   * @return true when substitution occurs, otherwise false
   */
  virtual bool SubstituteForLSP(PCUNICODE_STRING aLSPLeafName,
                                PHANDLE aOutHandle) = 0;

  /**
   * Notification that a DLL load has ended.
   *
   * @param aContext The context that was set by the corresponding call to
   *                 OnBeginDllLoad
   * @param aNtStatus The NTSTATUS returned by LdrLoadDll
   * @param aModuleLoadInfo Telemetry information that was gathered about the
   *                        load.
   */
  virtual void OnEndDllLoad(void* aContext, NTSTATUS aNtStatus,
                            ModuleLoadInfo&& aModuleLoadInfo) = 0;

  /**
   * Called to inform the observer that it is no longer active and, if
   * necessary, call aNext->OnForward() with any accumulated telemetry
   * information.
   */
  virtual void Forward(LoaderObserver* aNext) = 0;

  /**
   * Receives a vector of module load telemetry from a previous LoaderObserver.
   */
  virtual void OnForward(ModuleLoadInfoVec&& aInfo) = 0;
};

class NS_NO_VTABLE LoaderAPI {
 public:
  /**
   * Construct a new ModuleLoadInfo structure and notify the LoaderObserver
   * that a library load is beginning.
   */
  virtual ModuleLoadInfo ConstructAndNotifyBeginDllLoad(
      void** aContext, PCUNICODE_STRING aRequestedDllName) = 0;

  /**
   * Query to determine whether the DLL named |aLSPLeafName| needs to be
   * substituted with another module, and substitute the module handle when
   * necessary.
   *
   * @return true when substitution occurs, otherwise false
   */
  virtual bool SubstituteForLSP(PCUNICODE_STRING aLSPLeafName,
                                PHANDLE aOutHandle) = 0;

  /**
   * Notification that a DLL load has ended.
   */
  virtual void NotifyEndDllLoad(void* aContext, NTSTATUS aLoadNtStatus,
                                ModuleLoadInfo&& aModuleLoadInfo) = 0;

  /**
   * Given the address of a mapped section, obtain the name of the file that is
   * backing it.
   */
  virtual AllocatedUnicodeString GetSectionName(void* aSectionAddr) = 0;

  using InitDllBlocklistOOPFnPtr = LauncherVoidResultWithLineInfo (*)(
      const wchar_t*, HANDLE, const IMAGE_THUNK_DATA*);
  using HandleLauncherErrorFnPtr = void (*)(const LauncherError&, const char*);

  /**
   * Return a pointer to winlauncher's function.
   * Used by sandboxBroker::LaunchApp.
   */
  virtual InitDllBlocklistOOPFnPtr GetDllBlocklistInitFn() = 0;
  virtual HandleLauncherErrorFnPtr GetHandleLauncherErrorFn() = 0;
};

struct WinLauncherFunctions final {
  nt::LoaderAPI::InitDllBlocklistOOPFnPtr mInitDllBlocklistOOP;
  nt::LoaderAPI::HandleLauncherErrorFnPtr mHandleLauncherError;

  WinLauncherFunctions()
      : mInitDllBlocklistOOP(nullptr), mHandleLauncherError(nullptr) {}
};

}  // namespace nt
}  // namespace mozilla

#endif  // mozilla_LoaderAPIInterfaces_h
