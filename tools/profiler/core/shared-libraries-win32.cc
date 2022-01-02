/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "shared-libraries.h"
#include "nsWindowsHelpers.h"
#include "mozilla/NativeNt.h"
#include "mozilla/WindowsEnumProcessModules.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/WindowsVersion.h"
#include "nsPrintfCString.h"

static bool IsModuleUnsafeToLoad(const nsAString& aModuleName) {
#if defined(_M_AMD64) || defined(_M_IX86)
  // Hackaround for Bug 1607574.  Nvidia's shim driver nvd3d9wrap[x].dll detours
  // LoadLibraryExW and it causes AV when the following conditions are met.
  //   1. LoadLibraryExW was called for "detoured.dll"
  //   2. nvinit[x].dll was unloaded
  //   3. OS version is older than 6.2
#  if defined(_M_AMD64)
  LPCWSTR kNvidiaShimDriver = L"nvd3d9wrapx.dll";
  LPCWSTR kNvidiaInitDriver = L"nvinitx.dll";
#  elif defined(_M_IX86)
  LPCWSTR kNvidiaShimDriver = L"nvd3d9wrap.dll";
  LPCWSTR kNvidiaInitDriver = L"nvinit.dll";
#  endif
  if (aModuleName.LowerCaseEqualsLiteral("detoured.dll") &&
      !mozilla::IsWin8OrLater() && ::GetModuleHandleW(kNvidiaShimDriver) &&
      !::GetModuleHandleW(kNvidiaInitDriver)) {
    return true;
  }
#endif  // defined(_M_AMD64) || defined(_M_IX86)

  // Hackaround for Bug 1723868.  There is no safe way to prevent the module
  // Microsoft's VP9 Video Decoder from being unloaded because mfplat.dll may
  // have posted more than one task to unload the module in the work queue
  // without calling LoadLibrary.
  if (aModuleName.LowerCaseEqualsLiteral("msvp9dec_store.dll")) {
    return true;
  }

  return false;
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo sharedLibraryInfo;

  auto addSharedLibraryFromModuleInfo = [&sharedLibraryInfo](
                                            const wchar_t* aModulePath,
                                            HMODULE aModule) {
    nsDependentSubstring moduleNameStr(
        mozilla::nt::GetLeafName(nsDependentString(aModulePath)));

    // If the module is unsafe to call LoadLibraryEx for, we skip.
    if (IsModuleUnsafeToLoad(moduleNameStr)) {
      return;
    }

    // If EAF+ is enabled, parsing ntdll's PE header causes a crash.
    if (mozilla::IsEafPlusEnabled() &&
        moduleNameStr.LowerCaseEqualsLiteral("ntdll.dll")) {
      return;
    }

    // Load the module again to make sure that its handle will remain
    // valid as we attempt to read the PDB information from it.  We load the
    // DLL as a datafile so that we don't end up running the newly loaded
    // module's DllMain function.  If the original handle |aModule| is valid,
    // LoadLibraryEx just increments its refcount.
    // LOAD_LIBRARY_AS_IMAGE_RESOURCE is needed to read information from the
    // sections (not PE headers) which should be relocated by the loader,
    // otherwise GetPdbInfo() will cause a crash.
    nsModuleHandle handleLock(::LoadLibraryExW(
        aModulePath, NULL,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
    if (!handleLock) {
      return;
    }

    mozilla::nt::PEHeaders headers(handleLock.get());
    if (!headers) {
      return;
    }

    mozilla::Maybe<mozilla::Range<const uint8_t>> bounds = headers.GetBounds();
    if (!bounds) {
      return;
    }

    // Put the original |aModule| into SharedLibrary, but we get debug info
    // from |handleLock| as |aModule| might be inaccessible.
    const uintptr_t modStart = reinterpret_cast<uintptr_t>(aModule);
    const uintptr_t modEnd = modStart + bounds->length();

    nsAutoCString breakpadId;
    nsAutoString pdbPathStr;
    if (const auto* debugInfo = headers.GetPdbInfo()) {
      MOZ_ASSERT(breakpadId.IsEmpty());
      const GUID& pdbSig = debugInfo->pdbSignature;
      breakpadId.AppendPrintf(
          "%08X"                              // m0
          "%04X%04X"                          // m1,m2
          "%02X%02X%02X%02X%02X%02X%02X%02X"  // m3
          "%X",                               // pdbAge
          pdbSig.Data1, pdbSig.Data2, pdbSig.Data3, pdbSig.Data4[0],
          pdbSig.Data4[1], pdbSig.Data4[2], pdbSig.Data4[3], pdbSig.Data4[4],
          pdbSig.Data4[5], pdbSig.Data4[6], pdbSig.Data4[7], debugInfo->pdbAge);

      // The PDB file name could be different from module filename,
      // so report both
      // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
      pdbPathStr = NS_ConvertUTF8toUTF16(debugInfo->pdbFileName);
    }

    nsAutoCString versionStr;
    uint64_t version;
    if (headers.GetVersionInfo(version)) {
      versionStr.AppendPrintf("%u.%u.%u.%u",
                              static_cast<uint32_t>((version >> 48) & 0xFFFFu),
                              static_cast<uint32_t>((version >> 32) & 0xFFFFu),
                              static_cast<uint32_t>((version >> 16) & 0xFFFFu),
                              static_cast<uint32_t>(version & 0xFFFFu));
    }

    const nsString& pdbNameStr =
        PromiseFlatString(mozilla::nt::GetLeafName(pdbPathStr));
    SharedLibrary shlib(modStart, modEnd,
                        0,  // DLLs are always mapped at offset 0 on Windows
                        breakpadId, PromiseFlatString(moduleNameStr),
                        nsDependentString(aModulePath), pdbNameStr, pdbPathStr,
                        versionStr, "");
    sharedLibraryInfo.AddSharedLibrary(shlib);
  };

  mozilla::EnumerateProcessModules(addSharedLibraryFromModuleInfo);
  return sharedLibraryInfo;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
