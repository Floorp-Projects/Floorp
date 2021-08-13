/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsFallbackLoaderAPI_h
#define mozilla_WindowsFallbackLoaderAPI_h

#include "mozilla/Attributes.h"
#include "NtLoaderAPI.h"

namespace mozilla {

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS FallbackLoaderAPI final
    : public nt::LoaderAPI {
 public:
  constexpr FallbackLoaderAPI() : mLoaderObserver(nullptr) {}

  ModuleLoadInfo ConstructAndNotifyBeginDllLoad(
      void** aContext, PCUNICODE_STRING aRequestedDllName) final;
  bool SubstituteForLSP(PCUNICODE_STRING aLSPLeafName,
                        PHANDLE aOutHandle) final;
  void NotifyEndDllLoad(void* aContext, NTSTATUS aLoadNtStatus,
                        ModuleLoadInfo&& aModuleLoadInfo) final;
  nt::AllocatedUnicodeString GetSectionName(void* aSectionAddr) final;
  nt::LoaderAPI::InitDllBlocklistOOPFnPtr GetDllBlocklistInitFn() final;
  nt::LoaderAPI::HandleLauncherErrorFnPtr GetHandleLauncherErrorFn() final;

  void SetObserver(nt::LoaderObserver* aLoaderObserver);

 private:
  nt::LoaderObserver* mLoaderObserver;
};

}  // namespace mozilla

#endif  // mozilla_WindowsFallbackLoaderAPI_h
