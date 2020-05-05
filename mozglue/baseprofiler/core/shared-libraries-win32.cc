/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <dbghelp.h>
#include <sstream>
#include <psapi.h>

#include "BaseProfilerSharedLibraries.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include <string>

#define CV_SIGNATURE 0x53445352  // 'SDSR'

struct CodeViewRecord70 {
  uint32_t signature;
  GUID pdbSignature;
  uint32_t pdbAge;
  // A UTF-8 string, according to
  // https://github.com/Microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/dbi/locator.cpp#L785
  char pdbFileName[1];
};

static constexpr char digits[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static void AppendHex(const unsigned char* aBegin, const unsigned char* aEnd,
                      std::string& aOut) {
  for (const unsigned char* p = aBegin; p < aEnd; ++p) {
    unsigned char c = *p;
    aOut += digits[c >> 4];
    aOut += digits[c & 0xFu];
  }
}

static constexpr bool WITH_PADDING = true;
static constexpr bool WITHOUT_PADDING = false;
template <typename T>
static void AppendHex(T aValue, std::string& aOut, bool aWithPadding) {
  for (int i = sizeof(T) * 2 - 1; i >= 0; --i) {
    unsigned nibble = (aValue >> (i * 4)) & 0xFu;
    // If no-padding requested, skip starting zeroes -- unless we're on the very
    // last nibble (so we don't output a blank).
    if (!aWithPadding && i != 0) {
      if (nibble == 0) {
        // Requested no padding, skip zeroes.
        continue;
      }
      // Requested no padding, got first non-zero, pretend we now want padding
      // so we don't skip zeroes anymore.
      aWithPadding = true;
    }
    aOut += digits[nibble];
  }
}

static bool GetPdbInfo(uintptr_t aStart, std::string& aSignature,
                       uint32_t& aAge, char** aPdbName) {
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
  AppendHex(pdbSignature.Data1, aSignature, WITH_PADDING);
  AppendHex(pdbSignature.Data2, aSignature, WITH_PADDING);
  AppendHex(pdbSignature.Data3, aSignature, WITH_PADDING);
  AppendHex(reinterpret_cast<const unsigned char*>(&pdbSignature.Data4),
            reinterpret_cast<const unsigned char*>(&pdbSignature.Data4) +
                sizeof(pdbSignature.Data4),
            aSignature);

  // The PDB file name could be different from module filename, so report both
  // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
  *aPdbName = debugInfo->pdbFileName;

  return true;
}

static std::string GetVersion(char* dllPath) {
  DWORD infoSize = GetFileVersionInfoSizeA(dllPath, nullptr);
  if (infoSize == 0) {
    return {};
  }

  mozilla::UniquePtr<unsigned char[]> infoData =
      mozilla::MakeUnique<unsigned char[]>(infoSize);
  if (!GetFileVersionInfoA(dllPath, 0, infoSize, infoData.get())) {
    return {};
  }

  VS_FIXEDFILEINFO* vInfo;
  UINT vInfoLen;
  if (!VerQueryValueW(infoData.get(), L"\\", (LPVOID*)&vInfo, &vInfoLen)) {
    return {};
  }
  if (!vInfo) {
    return {};
  }

  return std::to_string(vInfo->dwFileVersionMS >> 16) + '.' +
         std::to_string(vInfo->dwFileVersionMS & 0xFFFF) + '.' +
         std::to_string(vInfo->dwFileVersionLS >> 16) + '.' +
         std::to_string(vInfo->dwFileVersionLS & 0xFFFF);
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
    std::string pdbPathStr;
    std::string pdbNameStr;
    char* pdbName = NULL;
    char modulePath[MAX_PATH + 1];

    if (!GetModuleFileNameEx(hProcess, hMods[i], modulePath,
                             sizeof(modulePath) / sizeof(char))) {
      continue;
    }

    MODULEINFO module = {0};
    if (!GetModuleInformation(hProcess, hMods[i], &module,
                              sizeof(MODULEINFO))) {
      continue;
    }

    std::string breakpadId;
    // Load the module again to make sure that its handle will remain
    // valid as we attempt to read the PDB information from it.  We load the
    // DLL as a datafile so that if the module actually gets unloaded between
    // the call to EnumProcessModules and the following LoadLibraryEx, we
    // don't end up running the now newly loaded module's DllMain function. If
    // the module is already loaded, LoadLibraryEx just increments its
    // refcount.
    //
    // Note that because of the race condition above, merely loading the DLL
    // again is not safe enough, therefore we also need to make sure that we
    // can read the memory mapped at the base address before we can safely
    // proceed to actually access those pages.
    HMODULE handleLock =
        LoadLibraryEx(modulePath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    MEMORY_BASIC_INFORMATION vmemInfo = {0};
    std::string pdbSig;
    uint32_t pdbAge;
    if (handleLock &&
        sizeof(vmemInfo) ==
            VirtualQuery(module.lpBaseOfDll, &vmemInfo, sizeof(vmemInfo)) &&
        vmemInfo.State == MEM_COMMIT &&
        GetPdbInfo((uintptr_t)module.lpBaseOfDll, pdbSig, pdbAge, &pdbName)) {
      MOZ_ASSERT(breakpadId.empty());
      breakpadId += pdbSig;
      AppendHex(pdbAge, breakpadId, WITHOUT_PADDING);

      pdbPathStr = pdbName;
      size_t pos = pdbPathStr.find_last_of("\\/");
      pdbNameStr =
          (pos != std::string::npos) ? pdbPathStr.substr(pos + 1) : pdbPathStr;
    }

    std::string modulePathStr(modulePath);
    size_t pos = modulePathStr.find_last_of("\\/");
    std::string moduleNameStr = (pos != std::string::npos)
                                    ? modulePathStr.substr(pos + 1)
                                    : modulePathStr;

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
