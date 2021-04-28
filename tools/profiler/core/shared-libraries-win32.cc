/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <dbghelp.h>
#include <sstream>
#include <psapi.h>

#include "shared-libraries.h"
#include "nsWindowsHelpers.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/WindowsVersion.h"
#include "nsNativeCharsetUtils.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"

#define CV_SIGNATURE 0x53445352  // 'SDSR'

struct CodeViewRecord70 {
  uint32_t signature;
  GUID pdbSignature;
  uint32_t pdbAge;
  // A UTF-8 string, according to
  // https://github.com/Microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/dbi/locator.cpp#L785
  char pdbFileName[1];
};

static bool GetPdbInfo(uintptr_t aStart, nsID& aSignature, uint32_t& aAge,
                       char** aPdbName) {
  if (!aStart) {
    return false;
  }

  PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(aStart);
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
    return false;
  }

  PIMAGE_NT_HEADERS ntHeaders =
      reinterpret_cast<PIMAGE_NT_HEADERS>(aStart + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
    return false;
  }

  uint32_t relativeVirtualAddress =
      ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG]
          .VirtualAddress;
  if (!relativeVirtualAddress) {
    return false;
  }

  PIMAGE_DEBUG_DIRECTORY debugDirectory =
      reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(aStart + relativeVirtualAddress);
  if (!debugDirectory || debugDirectory->Type != IMAGE_DEBUG_TYPE_CODEVIEW) {
    return false;
  }

  CodeViewRecord70* debugInfo = reinterpret_cast<CodeViewRecord70*>(
      aStart + debugDirectory->AddressOfRawData);
  if (!debugInfo || debugInfo->signature != CV_SIGNATURE) {
    return false;
  }

  aAge = debugInfo->pdbAge;
  GUID& pdbSignature = debugInfo->pdbSignature;
  aSignature.m0 = pdbSignature.Data1;
  aSignature.m1 = pdbSignature.Data2;
  aSignature.m2 = pdbSignature.Data3;
  memcpy(aSignature.m3, pdbSignature.Data4, sizeof(pdbSignature.Data4));

  // The PDB file name could be different from module filename, so report both
  // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
  *aPdbName = debugInfo->pdbFileName;

  return true;
}

static nsCString GetVersion(WCHAR* dllPath) {
  DWORD infoSize = GetFileVersionInfoSizeW(dllPath, nullptr);
  if (infoSize == 0) {
    return ""_ns;
  }

  mozilla::UniquePtr<unsigned char[]> infoData =
      mozilla::MakeUnique<unsigned char[]>(infoSize);
  if (!GetFileVersionInfoW(dllPath, 0, infoSize, infoData.get())) {
    return ""_ns;
  }

  VS_FIXEDFILEINFO* vInfo;
  UINT vInfoLen;
  if (!VerQueryValueW(infoData.get(), L"\\", (LPVOID*)&vInfo, &vInfoLen)) {
    return ""_ns;
  }
  if (!vInfo) {
    return ""_ns;
  }

  nsPrintfCString version("%d.%d.%d.%d", vInfo->dwFileVersionMS >> 16,
                          vInfo->dwFileVersionMS & 0xFFFF,
                          vInfo->dwFileVersionLS >> 16,
                          vInfo->dwFileVersionLS & 0xFFFF);
  return std::move(version);
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo sharedLibraryInfo;

  HANDLE hProcess = GetCurrentProcess();
  mozilla::UniquePtr<HMODULE[]> hMods;
  size_t modulesNum = 0;
  if (hProcess != NULL) {
    DWORD modulesSize;
    if (!EnumProcessModules(hProcess, nullptr, 0, &modulesSize)) {
      return sharedLibraryInfo;
    }
    modulesNum = modulesSize / sizeof(HMODULE);
    hMods = mozilla::MakeUnique<HMODULE[]>(modulesNum);
    if (!EnumProcessModules(hProcess, hMods.get(), modulesNum * sizeof(HMODULE),
                            &modulesSize)) {
      return sharedLibraryInfo;
    }
    // The list may have shrunk between calls
    if (modulesSize / sizeof(HMODULE) < modulesNum) {
      modulesNum = modulesSize / sizeof(HMODULE);
    }
  }

  for (unsigned int i = 0; i < modulesNum; i++) {
    WCHAR modulePath[MAX_PATH + 1];
    if (!GetModuleFileNameEx(hProcess, hMods[i], modulePath,
                             sizeof(modulePath) / sizeof(WCHAR))) {
      continue;
    }

    MODULEINFO module = {0};
    if (!GetModuleInformation(hProcess, hMods[i], &module,
                              sizeof(MODULEINFO))) {
      continue;
    }

    nsAutoString modulePathStr(modulePath);
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
      continue;
    }
#endif  // !defined(_M_ARM64)

    // If EAF+ is enabled, parsing ntdll's PE header via GetPdbInfo() causes
    // a crash.  We don't include PDB information in SharedLibrary.
    bool canGetPdbInfo = (!mozilla::IsEafPlusEnabled() ||
                          !moduleNameStr.LowerCaseEqualsLiteral("ntdll.dll"));

    nsCString breakpadId;
    // Load the module again to make sure that its handle will remain
    // valid as we attempt to read the PDB information from it.  We load the
    // DLL as a datafile so that if the module actually gets unloaded between
    // the call to EnumProcessModules and the following LoadLibraryEx, we don't
    // end up running the now newly loaded module's DllMain function.  If the
    // module is already loaded, LoadLibraryEx just increments its refcount.
    //
    // Note that because of the race condition above, merely loading the DLL
    // again is not safe enough, therefore we also need to make sure that we
    // can read the memory mapped at the base address before we can safely
    // proceed to actually access those pages.
    HMODULE handleLock =
        LoadLibraryEx(modulePath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    MEMORY_BASIC_INFORMATION vmemInfo = {0};
    nsID pdbSig;
    uint32_t pdbAge;
    nsAutoString pdbPathStr;
    nsAutoString pdbNameStr;
    char* pdbName = nullptr;
    if (handleLock &&
        sizeof(vmemInfo) ==
            VirtualQuery(module.lpBaseOfDll, &vmemInfo, sizeof(vmemInfo)) &&
        vmemInfo.State == MEM_COMMIT && canGetPdbInfo &&
        GetPdbInfo((uintptr_t)module.lpBaseOfDll, pdbSig, pdbAge, &pdbName)) {
      MOZ_ASSERT(breakpadId.IsEmpty());
      breakpadId.AppendPrintf(
          "%08X"                              // m0
          "%04X%04X"                          // m1,m2
          "%02X%02X%02X%02X%02X%02X%02X%02X"  // m3
          "%X",                               // pdbAge
          pdbSig.m0, pdbSig.m1, pdbSig.m2, pdbSig.m3[0], pdbSig.m3[1],
          pdbSig.m3[2], pdbSig.m3[3], pdbSig.m3[4], pdbSig.m3[5], pdbSig.m3[6],
          pdbSig.m3[7], pdbAge);

      pdbPathStr = NS_ConvertUTF8toUTF16(pdbName);
      pdbNameStr = pdbPathStr;
      int32_t pos = pdbNameStr.RFindCharInSet(u"\\/");
      if (pos != kNotFound) {
        pdbNameStr.Cut(0, pos + 1);
      }
    }

    SharedLibrary shlib((uintptr_t)module.lpBaseOfDll,
                        (uintptr_t)module.lpBaseOfDll + module.SizeOfImage,
                        0,  // DLLs are always mapped at offset 0 on Windows
                        breakpadId, moduleNameStr, modulePathStr, pdbNameStr,
                        pdbPathStr, GetVersion(modulePath), "");
    sharedLibraryInfo.AddSharedLibrary(shlib);

    FreeLibrary(handleLock);  // ok to free null handles
  }

  return sharedLibraryInfo;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
