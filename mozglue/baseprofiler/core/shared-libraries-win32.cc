/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "BaseProfilerSharedLibraries.h"

#include "mozilla/glue/WindowsUnicode.h"
#include "mozilla/NativeNt.h"
#include "mozilla/WindowsEnumProcessModules.h"
#include "mozilla/WindowsVersion.h"

#include <cctype>
#include <string>

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

// Hackaround for Bug 1607574.  Nvidia's shim driver nvd3d9wrap[x].dll detours
// LoadLibraryExW and it causes AV when the following conditions are met.
//   1. LoadLibraryExW was called for "detoured.dll"
//   2. nvinit[x].dll was unloaded
//   3. OS version is older than 6.2
static bool IsModuleUnsafeToLoad(const std::string& aModuleName) {
#if defined(_M_ARM64)
  return false;
#else
#  if defined(_M_AMD64)
  LPCWSTR kNvidiaShimDriver = L"nvd3d9wrapx.dll";
  LPCWSTR kNvidiaInitDriver = L"nvinitx.dll";
#  elif defined(_M_IX86)
  LPCWSTR kNvidiaShimDriver = L"nvd3d9wrap.dll";
  LPCWSTR kNvidiaInitDriver = L"nvinit.dll";
#  endif
  constexpr std::string_view detoured_dll = "detoured.dll";
  return std::equal(aModuleName.cbegin(), aModuleName.cend(),
                    detoured_dll.cbegin(), detoured_dll.cend(),
                    [](char aModuleChar, char aDetouredChar) {
                      return std::tolower(aModuleChar) == aDetouredChar;
                    }) &&
         !mozilla::IsWin8OrLater() && ::GetModuleHandleW(kNvidiaShimDriver) &&
         !::GetModuleHandleW(kNvidiaInitDriver);
#endif  // defined(_M_ARM64)
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo sharedLibraryInfo;

  auto addSharedLibraryFromModuleInfo =
      [&sharedLibraryInfo](const wchar_t* aModulePath, HMODULE aModule) {
        mozilla::UniquePtr<char[]> utf8ModulePath(
            mozilla::glue::WideToUTF8(aModulePath));
        if (!utf8ModulePath) {
          return;
        }

        std::string modulePathStr(utf8ModulePath.get());
        size_t pos = modulePathStr.find_last_of("\\/");
        std::string moduleNameStr = (pos != std::string::npos)
                                        ? modulePathStr.substr(pos + 1)
                                        : modulePathStr;

        // If the module is unsafe to call LoadLibraryEx for, we skip.
        if (IsModuleUnsafeToLoad(moduleNameStr)) {
          return;
        }

        // Load the module again to make sure that its handle will remain
        // valid as we attempt to read the PDB information from it.  We load the
        // DLL as a datafile so that we don't end up running the newly loaded
        // module's DllMain function.  If the original handle |aModule| is
        // valid, LoadLibraryEx just increments its refcount.
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

        mozilla::Maybe<mozilla::Range<const uint8_t>> bounds =
            headers.GetBounds();
        if (!bounds) {
          return;
        }

        // Put the original |aModule| into SharedLibrary, but we get debug info
        // from |handleLock| as |aModule| might be inaccessible.
        const uintptr_t modStart = reinterpret_cast<uintptr_t>(aModule);
        const uintptr_t modEnd = modStart + bounds->length();

        std::string breakpadId;
        std::string pdbPathStr;
        std::string pdbNameStr;
        if (const auto* debugInfo = headers.GetPdbInfo()) {
          MOZ_ASSERT(breakpadId.empty());
          const GUID& pdbSig = debugInfo->pdbSignature;
          AppendHex(pdbSig.Data1, breakpadId, WITH_PADDING);
          AppendHex(pdbSig.Data2, breakpadId, WITH_PADDING);
          AppendHex(pdbSig.Data3, breakpadId, WITH_PADDING);
          AppendHex(reinterpret_cast<const unsigned char*>(&pdbSig.Data4),
                    reinterpret_cast<const unsigned char*>(&pdbSig.Data4) +
                        sizeof(pdbSig.Data4),
                    breakpadId);
          AppendHex(debugInfo->pdbAge, breakpadId, WITHOUT_PADDING);

          // The PDB file name could be different from module filename,
          // so report both
          // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
          pdbPathStr = debugInfo->pdbFileName;
          size_t pos = pdbPathStr.find_last_of("\\/");
          pdbNameStr = (pos != std::string::npos) ? pdbPathStr.substr(pos + 1)
                                                  : pdbPathStr;
        }

        std::string versionStr;
        uint64_t version;
        if (headers.GetVersionInfo(version)) {
          versionStr += std::to_string((version >> 48) & 0xFFFF);
          versionStr += '.';
          versionStr += std::to_string((version >> 32) & 0xFFFF);
          versionStr += '.';
          versionStr += std::to_string((version >> 16) & 0xFFFF);
          versionStr += '.';
          versionStr += std::to_string(version & 0xFFFF);
        }

        SharedLibrary shlib(modStart, modEnd,
                            0,  // DLLs are always mapped at offset 0 on Windows
                            breakpadId, moduleNameStr, modulePathStr,
                            pdbNameStr, pdbPathStr, versionStr, "");
        sharedLibraryInfo.AddSharedLibrary(shlib);
      };

  mozilla::EnumerateProcessModules(addSharedLibraryFromModuleInfo);
  return sharedLibraryInfo;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
