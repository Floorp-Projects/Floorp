/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleLoadFrame.h"
#include "mozilla/NativeNt.h"
#include "mozilla/UniquePtr.h"
#include "NtLoaderAPI.h"

#include <string.h>

#include "WindowsFallbackLoaderAPI.h"

static bool IsNullTerminated(PCUNICODE_STRING aStr) {
  return aStr && (aStr->MaximumLength >= (aStr->Length + sizeof(WCHAR))) &&
         aStr->Buffer && aStr->Buffer[aStr->Length / sizeof(WCHAR)] == 0;
}

static mozilla::FallbackLoaderAPI gFallbackLoaderAPI;

namespace mozilla {
namespace glue {

nt::LoaderAPI* ModuleLoadFrame::sLoaderAPI;

using GetNtLoaderAPIFn = decltype(&mozilla::GetNtLoaderAPI);

/* static */
void ModuleLoadFrame::StaticInit(
    nt::LoaderObserver* aNewObserver,
    nt::WinLauncherFunctions* aOutWinLauncherFunctions) {
  const auto pGetNtLoaderAPI = reinterpret_cast<GetNtLoaderAPIFn>(
      ::GetProcAddress(::GetModuleHandleW(nullptr), "GetNtLoaderAPI"));
  if (!pGetNtLoaderAPI) {
    // This case occurs in processes other than firefox.exe that do not contain
    // the launcher process blocklist.
    gFallbackLoaderAPI.SetObserver(aNewObserver);
    sLoaderAPI = &gFallbackLoaderAPI;

    if (aOutWinLauncherFunctions) {
      aOutWinLauncherFunctions->mHandleLauncherError =
          [](const mozilla::LauncherError&, const char*) {};
      // We intentionally leave mInitDllBlocklistOOP null to make sure calling
      // mInitDllBlocklistOOP in non-Firefox hits MOZ_RELEASE_ASSERT.
    }
    return;
  }

  sLoaderAPI = pGetNtLoaderAPI(aNewObserver);
  MOZ_ASSERT(sLoaderAPI);

  if (aOutWinLauncherFunctions) {
    aOutWinLauncherFunctions->mInitDllBlocklistOOP =
        sLoaderAPI->GetDllBlocklistInitFn();
    aOutWinLauncherFunctions->mHandleLauncherError =
        sLoaderAPI->GetHandleLauncherErrorFn();
  }
}

ModuleLoadFrame::ModuleLoadFrame(PCUNICODE_STRING aRequestedDllName)
    : mAlreadyLoaded(false),
      mContext(nullptr),
      mDllLoadStatus(STATUS_UNSUCCESSFUL),
      mLoadInfo(sLoaderAPI->ConstructAndNotifyBeginDllLoad(&mContext,
                                                           aRequestedDllName)) {
  if (!aRequestedDllName) {
    return;
  }

  UniquePtr<WCHAR[]> nameBuf;
  const WCHAR* name = nullptr;

  if (IsNullTerminated(aRequestedDllName)) {
    name = aRequestedDllName->Buffer;
  } else {
    USHORT charLenExclNul = aRequestedDllName->Length / sizeof(WCHAR);
    USHORT charLenInclNul = charLenExclNul + 1;
    nameBuf = MakeUnique<WCHAR[]>(charLenInclNul);
    if (!wcsncpy_s(nameBuf.get(), charLenInclNul, aRequestedDllName->Buffer,
                   charLenExclNul)) {
      name = nameBuf.get();
    }
  }

  mAlreadyLoaded = name && !!::GetModuleHandleW(name);
}

ModuleLoadFrame::~ModuleLoadFrame() {
  sLoaderAPI->NotifyEndDllLoad(mContext, mDllLoadStatus, std::move(mLoadInfo));
}

void ModuleLoadFrame::SetLoadStatus(NTSTATUS aNtStatus, HANDLE aHandle) {
  mDllLoadStatus = aNtStatus;
  void* baseAddr = mozilla::nt::PEHeaders::HModuleToBaseAddr<void*>(
      reinterpret_cast<HMODULE>(aHandle));
  mLoadInfo.mBaseAddr = baseAddr;
  if (!mAlreadyLoaded) {
    mLoadInfo.mSectionName = sLoaderAPI->GetSectionName(baseAddr);
  }
}

}  // namespace glue
}  // namespace mozilla
