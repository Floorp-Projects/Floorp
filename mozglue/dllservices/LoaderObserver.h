/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_LoaderObserver_h
#define mozilla_glue_LoaderObserver_h

#include "mozilla/Attributes.h"
#include "mozilla/LoaderAPIInterfaces.h"
#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/glue/WinUtils.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace glue {

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS LoaderObserver final
    : public nt::LoaderObserver {
 public:
  constexpr LoaderObserver() : mModuleLoads(nullptr), mEnabled(true) {}

  void OnBeginDllLoad(void** aContext,
                      PCUNICODE_STRING aPreliminaryDllName) final;
  bool SubstituteForLSP(PCUNICODE_STRING aLspLeafName,
                        PHANDLE aOutHandle) final;
  void OnEndDllLoad(void* aContext, NTSTATUS aNtStatus,
                    ModuleLoadInfo&& aModuleLoadInfo) final;
  void Forward(nt::LoaderObserver* aNext) final;
  void OnForward(ModuleLoadInfoVec&& aInfo) final;

  void Forward(mozilla::glue::detail::DllServicesBase* aSvc);
  void Disable();

 private:
  Win32SRWLock mLock;
  ModuleLoadInfoVec* mModuleLoads;
  bool mEnabled;
};

}  // namespace glue
}  // namespace mozilla

#endif  // mozilla_glue_LoaderObserver_h
