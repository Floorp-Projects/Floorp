/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <psapi.h>

#include "shared-libraries.h"
#include "nsWindowsHelpers.h"
#include "mozilla/NativeNt.h"
#include "mozilla/WindowsEnumProcessModules.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/WindowsVersion.h"
#include "nsPrintfCString.h"

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo sharedLibraryInfo;

  auto addSharedLibraryFromModuleInfo = [&sharedLibraryInfo](
                                            const wchar_t* aModulePath,
                                            HMODULE aModule) {
    MODULEINFO module = {0};
    if (!GetModuleInformation(mozilla::nt::kCurrentProcess, aModule, &module,
                              sizeof(MODULEINFO))) {
      return;
    }

    nsAutoString modulePathStr(aModulePath);
    nsAutoString moduleNameStr = modulePathStr;
    int32_t pos = moduleNameStr.RFindCharInSet(u"\\/");
    if (pos != kNotFound) {
      moduleNameStr.Cut(0, pos + 1);
    }

    // Hackaround for Bug 1607574.  Nvidia's shim driver nvd3d9wrap[x].dll
    // detours LoadLibraryExW when it's loaded and the detour function causes
    // AV when the code tries to access data pointing to an address within
    // unloaded nvinit[x].dll.
    // The crashing code is executed when a given parameter is "detoured.dll"
    // and OS version is older than 6.2.  We hit that crash at the following
    // call to LoadLibraryEx even if we specify LOAD_LIBRARY_AS_DATAFILE.
    // We work around it by skipping LoadLibraryEx, and add a library info with
    // a dummy breakpad id instead.
#if !defined(_M_ARM64)
#  if defined(_M_AMD64)
    LPCWSTR kNvidiaShimDriver = L"nvd3d9wrapx.dll";
    LPCWSTR kNvidiaInitDriver = L"nvinitx.dll";
#  elif defined(_M_IX86)
    LPCWSTR kNvidiaShimDriver = L"nvd3d9wrap.dll";
    LPCWSTR kNvidiaInitDriver = L"nvinit.dll";
#  endif
    if (moduleNameStr.LowerCaseEqualsLiteral("detoured.dll") &&
        !mozilla::IsWin8OrLater() && ::GetModuleHandle(kNvidiaShimDriver) &&
        !::GetModuleHandle(kNvidiaInitDriver)) {
      constexpr auto pdbNameStr = u"detoured.pdb"_ns;
      SharedLibrary shlib((uintptr_t)module.lpBaseOfDll,
                          (uintptr_t)module.lpBaseOfDll + module.SizeOfImage,
                          0,  // DLLs are always mapped at offset 0 on Windows
                          "000000000000000000000000000000000"_ns, moduleNameStr,
                          modulePathStr, pdbNameStr, pdbNameStr, ""_ns, "");
      sharedLibraryInfo.AddSharedLibrary(shlib);
      return;
    }
#endif  // !defined(_M_ARM64)

    // If EAF+ is enabled, parsing ntdll's PE header via GetPdbInfo() causes
    // a crash.  We don't include PDB information in SharedLibrary.
    bool canGetPdbInfo = (!mozilla::IsEafPlusEnabled() ||
                          !moduleNameStr.LowerCaseEqualsLiteral("ntdll.dll"));

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

    nsAutoCString breakpadId;
    nsAutoCString versionStr;
    nsAutoString pdbPathStr;
    nsAutoString pdbNameStr;
    if (handleLock && canGetPdbInfo) {
      mozilla::nt::PEHeaders headers(handleLock.get());
      if (headers) {
        if (const auto* debugInfo = headers.GetPdbInfo()) {
          MOZ_ASSERT(breakpadId.IsEmpty());
          const GUID& pdbSig = debugInfo->pdbSignature;
          breakpadId.AppendPrintf(
              "%08X"                              // m0
              "%04X%04X"                          // m1,m2
              "%02X%02X%02X%02X%02X%02X%02X%02X"  // m3
              "%X",                               // pdbAge
              pdbSig.Data1, pdbSig.Data2, pdbSig.Data3, pdbSig.Data4[0],
              pdbSig.Data4[1], pdbSig.Data4[2], pdbSig.Data4[3],
              pdbSig.Data4[4], pdbSig.Data4[5], pdbSig.Data4[6],
              pdbSig.Data4[7], debugInfo->pdbAge);

          // The PDB file name could be different from module filename,
          // so report both
          // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
          pdbPathStr = NS_ConvertUTF8toUTF16(debugInfo->pdbFileName);
          pdbNameStr = pdbPathStr;
          int32_t pos = pdbNameStr.RFindCharInSet(u"\\/");
          if (pos != kNotFound) {
            pdbNameStr.Cut(0, pos + 1);
          }
        }

        uint64_t version;
        if (headers.GetVersionInfo(version)) {
          versionStr.AppendPrintf("%d.%d.%d.%d", (version >> 48) & 0xFFFF,
                                  (version >> 32) & 0xFFFF,
                                  (version >> 16) & 0xFFFF, version & 0xFFFF);
        }
      }
    }

    SharedLibrary shlib((uintptr_t)module.lpBaseOfDll,
                        (uintptr_t)module.lpBaseOfDll + module.SizeOfImage,
                        0,  // DLLs are always mapped at offset 0 on Windows
                        breakpadId, moduleNameStr, modulePathStr, pdbNameStr,
                        pdbPathStr, versionStr, "");
    sharedLibraryInfo.AddSharedLibrary(shlib);
  };

  mozilla::EnumerateProcessModules(addSharedLibraryFromModuleInfo);
  return sharedLibraryInfo;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
