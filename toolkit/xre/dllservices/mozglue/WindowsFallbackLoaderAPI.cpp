/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WindowsFallbackLoaderAPI.h"

namespace mozilla {

ModuleLoadInfo FallbackLoaderAPI::ConstructAndNotifyBeginDllLoad(
    void** aContext, PCUNICODE_STRING aRequestedDllName) {
  ModuleLoadInfo loadInfo(aRequestedDllName);

  MOZ_ASSERT(mLoaderObserver);
  if (mLoaderObserver) {
    mLoaderObserver->OnBeginDllLoad(aContext, aRequestedDllName);
  }

  return loadInfo;
}

bool FallbackLoaderAPI::SubstituteForLSP(PCUNICODE_STRING aLSPLeafName,
                                         PHANDLE aOutHandle) {
  MOZ_ASSERT(mLoaderObserver);
  if (!mLoaderObserver) {
    return false;
  }

  return mLoaderObserver->SubstituteForLSP(aLSPLeafName, aOutHandle);
}

void FallbackLoaderAPI::NotifyEndDllLoad(void* aContext, NTSTATUS aLoadNtStatus,
                                         ModuleLoadInfo&& aModuleLoadInfo) {
  aModuleLoadInfo.SetEndLoadTimeStamp();

  if (NT_SUCCESS(aLoadNtStatus)) {
    aModuleLoadInfo.CaptureBacktrace();
  }

  MOZ_ASSERT(mLoaderObserver);
  if (mLoaderObserver) {
    mLoaderObserver->OnEndDllLoad(aContext, aLoadNtStatus,
                                  std::move(aModuleLoadInfo));
  }
}

nt::AllocatedUnicodeString FallbackLoaderAPI::GetSectionName(
    void* aSectionAddr) {
  static const StaticDynamicallyLinkedFunctionPtr<
      decltype(&::NtQueryVirtualMemory)>
      pNtQueryVirtualMemory(L"ntdll.dll", "NtQueryVirtualMemory");
  MOZ_ASSERT(pNtQueryVirtualMemory);

  if (!pNtQueryVirtualMemory) {
    return nt::AllocatedUnicodeString();
  }

  nt::MemorySectionNameBuf buf;
  NTSTATUS ntStatus =
      pNtQueryVirtualMemory(::GetCurrentProcess(), aSectionAddr,
                            MemorySectionName, &buf, sizeof(buf), nullptr);
  if (!NT_SUCCESS(ntStatus)) {
    return nt::AllocatedUnicodeString();
  }

  return nt::AllocatedUnicodeString(&buf.mSectionFileName);
}

nt::LoaderAPI::InitDllBlocklistOOPFnPtr
FallbackLoaderAPI::GetDllBlocklistInitFn() {
  MOZ_ASSERT_UNREACHABLE("This should not be called so soon!");
  return nullptr;
}

nt::LoaderAPI::HandleLauncherErrorFnPtr
FallbackLoaderAPI::GetHandleLauncherErrorFn() {
  MOZ_ASSERT_UNREACHABLE("This should not be called so soon!");
  return nullptr;
}

void FallbackLoaderAPI::SetObserver(nt::LoaderObserver* aLoaderObserver) {
  mLoaderObserver = aLoaderObserver;
}

}  // namespace mozilla
