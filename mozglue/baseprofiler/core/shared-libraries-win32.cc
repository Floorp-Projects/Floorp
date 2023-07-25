/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "BaseProfilerSharedLibraries.h"

#include "mozilla/glue/WindowsUnicode.h"
#include "mozilla/NativeNt.h"
#include "mozilla/WindowsEnumProcessModules.h"

#include <cctype>
#include <string>

static constexpr char uppercaseDigits[16] = {'0', '1', '2', '3', '4', '5',
                                             '6', '7', '8', '9', 'A', 'B',
                                             'C', 'D', 'E', 'F'};
static constexpr char lowercaseDigits[16] = {'0', '1', '2', '3', '4', '5',
                                             '6', '7', '8', '9', 'a', 'b',
                                             'c', 'd', 'e', 'f'};

static void AppendHex(const unsigned char* aBegin, const unsigned char* aEnd,
                      std::string& aOut) {
  for (const unsigned char* p = aBegin; p < aEnd; ++p) {
    unsigned char c = *p;
    aOut += uppercaseDigits[c >> 4];
    aOut += uppercaseDigits[c & 0xFu];
  }
}

static constexpr bool WITH_PADDING = true;
static constexpr bool WITHOUT_PADDING = false;
static constexpr bool LOWERCASE = true;
static constexpr bool UPPERCASE = false;
template <typename T>
static void AppendHex(T aValue, std::string& aOut, bool aWithPadding,
                      bool aLowercase = UPPERCASE) {
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
    aOut += aLowercase ? lowercaseDigits[nibble] : uppercaseDigits[nibble];
  }
}

static bool IsModuleUnsafeToLoad(const std::string& aModuleName) {
  auto LowerCaseEqualsLiteral = [](char aModuleChar, char aDetouredChar) {
    return std::tolower(aModuleChar) == aDetouredChar;
  };

  // Hackaround for Bug 1723868.  There is no safe way to prevent the module
  // Microsoft's VP9 Video Decoder from being unloaded because mfplat.dll may
  // have posted more than one task to unload the module in the work queue
  // without calling LoadLibrary.
  constexpr std::string_view vp9_decoder_dll = "msvp9dec_store.dll";
  if (std::equal(aModuleName.cbegin(), aModuleName.cend(),
                 vp9_decoder_dll.cbegin(), vp9_decoder_dll.cend(),
                 LowerCaseEqualsLiteral)) {
    return true;
  }

  return false;
}

void SharedLibraryInfo::AddSharedLibraryFromModuleInfo(
    const wchar_t* aModulePath, mozilla::Maybe<HMODULE> aModule) {
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

  // Load the module again - to make sure that its handle will remain valid as
  // we attempt to read the PDB information from it - or for the first time if
  // we only have a path. We want to load the DLL without running the newly
  // loaded module's DllMain function, but not as a data file because we want
  // to be able to do RVA computations easily. Hence, we use the flag
  // LOAD_LIBRARY_AS_IMAGE_RESOURCE which ensures that the sections (not PE
  // headers) will be relocated by the loader. Otherwise GetPdbInfo() and/or
  // GetVersionInfo() can cause a crash. If the original handle |aModule| is
  // valid, LoadLibraryEx just increments its refcount.
  nsModuleHandle handleLock(
      ::LoadLibraryExW(aModulePath, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE));
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
  const uintptr_t modStart =
      aModule.isSome() ? reinterpret_cast<uintptr_t>(*aModule)
                       : reinterpret_cast<uintptr_t>(handleLock.get());
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
    pdbNameStr =
        (pos != std::string::npos) ? pdbPathStr.substr(pos + 1) : pdbPathStr;
  }

  std::string codeId;
  DWORD timestamp;
  DWORD imageSize;
  if (headers.GetTimeStamp(timestamp) && headers.GetImageSize(imageSize)) {
    AppendHex(timestamp, codeId, WITH_PADDING);
    AppendHex(imageSize, codeId, WITHOUT_PADDING, LOWERCASE);
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
                      breakpadId, codeId, moduleNameStr, modulePathStr,
                      pdbNameStr, pdbPathStr, versionStr, "");
  AddSharedLibrary(shlib);
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo sharedLibraryInfo;

  auto addSharedLibraryFromModuleInfo =
      [&sharedLibraryInfo](const wchar_t* aModulePath, HMODULE aModule) {
        sharedLibraryInfo.AddSharedLibraryFromModuleInfo(
            aModulePath, mozilla::Some(aModule));
      };

  mozilla::EnumerateProcessModules(addSharedLibraryFromModuleInfo);
  return sharedLibraryInfo;
}

SharedLibraryInfo SharedLibraryInfo::GetInfoFromPath(const wchar_t* aPath) {
  SharedLibraryInfo sharedLibraryInfo;
  sharedLibraryInfo.AddSharedLibraryFromModuleInfo(aPath, mozilla::Nothing());
  return sharedLibraryInfo;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
