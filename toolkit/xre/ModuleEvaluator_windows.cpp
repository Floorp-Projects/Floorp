/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ModuleEvaluator_windows.h"

#include <windows.h>
#include <algorithm> // For std::find()

#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsReadableUtils.h"
#include "nsWindowsHelpers.h"
#include "WinUtils.h"

namespace mozilla {

// Utility function to get the parent directory of aFile.
// Returns true upon success.
static bool
GetDirectoryName(const nsCOMPtr<nsIFile> aFile, nsAString& aParent)
{
  nsCOMPtr<nsIFile> parentDir;
  if (NS_FAILED(aFile->GetParent(getter_AddRefs(parentDir))) || !parentDir) {
    return false;
  }
  if (NS_FAILED(parentDir->GetPath(aParent))) {
    return false;
  }
  return true;
}

ModuleLoadEvent::ModuleInfo::ModuleInfo(const glue::ModuleLoadEvent::ModuleInfo& aOther)
  : mBase(aOther.mBase)
{
  if (aOther.mLdrName) {
    mLdrName.Assign(aOther.mLdrName.get());
  }
  if (aOther.mFullPath) {
    nsDependentString tempPath(aOther.mFullPath.get());
    Unused << NS_NewLocalFile(tempPath, false, getter_AddRefs(mFile));
  }
}

ModuleLoadEvent::ModuleLoadEvent(const ModuleLoadEvent& aOther, CopyOption aOption)
  : mIsStartup(aOther.mIsStartup)
  , mThreadID(aOther.mThreadID)
  , mThreadName(aOther.mThreadName)
  , mProcessUptimeMS(aOther.mProcessUptimeMS)
{
  Unused << mStack.reserve(aOther.mStack.length());
  for (auto& x : aOther.mStack) {
    Unused << mStack.append(x);
  }
  if (aOption != CopyOption::CopyWithoutModules) {
    Unused << mModules.reserve(aOther.mModules.length());
    for (auto& x : aOther.mModules) {
      Unused << mModules.append(x);
    }
  }
}

ModuleLoadEvent::ModuleLoadEvent(const glue::ModuleLoadEvent& aOther)
  : mIsStartup(false) // Events originating in glue:: cannot be a startup event.
  , mThreadID(aOther.mThreadID)
  , mProcessUptimeMS(aOther.mProcessUptimeMS)
{
  for (auto& frame : aOther.mStack) {
    Unused << mStack.append(frame);
  }
  for (auto& module : aOther.mModules) {
    Unused << mModules.append(ModuleInfo(module));
  }
}

// Fills a Vector with keyboard layout DLLs found in the registry.
// These are leaf names only, not full paths. Here we will convert them to
// lowercase before returning, to facilitate case-insensitive searches.
// On error, this may return partial results.
static void
GetKeyboardLayoutDlls(Vector<nsString, 0, InfallibleAllocPolicy>& aOut)
{
  HKEY rawKey;
  if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts",
                      0, KEY_ENUMERATE_SUB_KEYS, &rawKey) != ERROR_SUCCESS) {
    return;
  }
  nsAutoRegKey key(rawKey);

  DWORD iKey = 0;
  wchar_t strTemp[MAX_PATH] = {0};
  while (true) {
    DWORD strTempSize = ArrayLength(strTemp);
    if (RegEnumKeyExW(rawKey, iKey, strTemp, &strTempSize,
                      nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      return; // ERROR_NO_MORE_ITEMS or a real error: bail with what we have.
    }
    iKey++;

    strTempSize = sizeof(strTemp);
    if (::RegGetValueW(rawKey, strTemp, L"Layout File",
                       RRF_RT_REG_SZ, nullptr, strTemp,
                       &strTempSize) == ERROR_SUCCESS) {
      nsString ws(strTemp, (strTempSize / sizeof(wchar_t)) - 1);
      ToLowerCase(ws); // To facilitate searches
      Unused << aOut.emplaceBack(ws);
    }
  }
}

ModuleEvaluator::ModuleEvaluator()
{
  GetKeyboardLayoutDlls(mKeyboardLayoutDlls);

  nsCOMPtr<nsIFile> sysDir;
  if (NS_SUCCEEDED(NS_GetSpecialDirectory(NS_OS_SYSTEM_DIR,
                                          getter_AddRefs(sysDir)))) {
    sysDir->GetPath(mSysDirectory);
  }

  nsCOMPtr<nsIFile> exeDir;
  if (NS_SUCCEEDED(NS_GetSpecialDirectory(NS_GRE_DIR,
                                          getter_AddRefs(exeDir)))) {
    exeDir->GetPath(mExeDirectory);
  }

  nsCOMPtr<nsIFile> exeFile;
  if (NS_SUCCEEDED(XRE_GetBinaryPath(getter_AddRefs(exeFile)))) {
    nsAutoString exePath;
    if (NS_SUCCEEDED(exeFile->GetPath(exePath))) {
      ModuleVersionInfo exeVi;
      if (exeVi.GetFromImage(exePath)) {
        mExeVersion = Some(exeVi.mFileVersion.Version64());
      }
    }
  }
}

Maybe<bool>
ModuleEvaluator::IsModuleTrusted(ModuleLoadEvent::ModuleInfo& aDllInfo,
                                 const ModuleLoadEvent& aEvent,
                                 Authenticode* aSvc) const
{
  // The JIT profiling module doesn't really have any other practical way to
  // match; hard-code it as being trusted.
  if (aDllInfo.mLdrName.EqualsLiteral("JitPI.dll")) {
    aDllInfo.mTrustFlags = ModuleTrustFlags::JitPI;
    return Some(true);
  }

  aDllInfo.mTrustFlags = ModuleTrustFlags::None;

  if (!aDllInfo.mFile) {
    return Nothing(); // Every check here depends on having a valid image file.
  }

  using PathTransformFlags = widget::WinUtils::PathTransformFlags;

  Unused << widget::WinUtils::PreparePathForTelemetry(aDllInfo.mLdrName,
      PathTransformFlags::Default & ~PathTransformFlags::Canonicalize);

  nsAutoString dllFullPath;
  if (NS_FAILED(aDllInfo.mFile->GetPath(dllFullPath))) {
    return Nothing();
  }
  widget::WinUtils::MakeLongPath(dllFullPath);

  aDllInfo.mFilePathClean = dllFullPath;
  if (!widget::WinUtils::PreparePathForTelemetry(aDllInfo.mFilePathClean,
      PathTransformFlags::Default &
      ~(PathTransformFlags::Canonicalize | PathTransformFlags::Lengthen))) {
    return Nothing();
  }

  if (NS_FAILED(NS_NewLocalFile(dllFullPath, false,
                                getter_AddRefs(aDllInfo.mFile)))) {
    return Nothing();
  }

  nsAutoString dllDirectory;
  if (!GetDirectoryName(aDllInfo.mFile, dllDirectory)) {
    return Nothing();
  }

  nsAutoString dllLeafLower;
  if (NS_FAILED(aDllInfo.mFile->GetLeafName(dllLeafLower))) {
    return Nothing();
  }
  ToLowerCase(dllLeafLower); // To facilitate case-insensitive searching

#if ENABLE_TESTS
  int scoreThreshold = 100;
  // For testing, these DLLs are hardcoded to pass through all criteria checks
  // and still result in "untrusted" status.
  if (dllLeafLower.EqualsLiteral("mozglue.dll") ||
      dllLeafLower.EqualsLiteral("modules-test.dll")) {
    scoreThreshold = 99999;
  }
#else
  static const int scoreThreshold = 100;
#endif

  int score = 0;

  // Is the DLL in the system directory?
  if (!mSysDirectory.IsEmpty() && StringBeginsWith(dllFullPath, mSysDirectory,
                                                   nsCaseInsensitiveStringComparator())) {
    aDllInfo.mTrustFlags |= ModuleTrustFlags::SystemDirectory;
    score += 50;
  }

  // Is it a keyboard layout DLL?
  if (std::find(mKeyboardLayoutDlls.begin(),
                mKeyboardLayoutDlls.end(),
                dllLeafLower) != mKeyboardLayoutDlls.end()) {
    aDllInfo.mTrustFlags |= ModuleTrustFlags::KeyboardLayout;
    // This doesn't guarantee trustworthiness by itself. Keyboard layouts also
    // must be in the system directory, which will bump the score >= 100.
    score += 50;
  }

  if (score < scoreThreshold) {
    ModuleVersionInfo vi;
    if (vi.GetFromImage(dllFullPath)) {
      aDllInfo.mFileVersion = vi.mFileVersion.ToString();

      if (vi.mCompanyName.EqualsLiteral("Microsoft Corporation")) {
        aDllInfo.mTrustFlags |= ModuleTrustFlags::MicrosoftVersion;
        score += 50;
      }

      if (!mExeDirectory.IsEmpty() && StringBeginsWith(dllFullPath, mExeDirectory,
                                                       nsCaseInsensitiveStringComparator())) {
        score += 50;
        aDllInfo.mTrustFlags |= ModuleTrustFlags::FirefoxDirectory;

        // If it's in the Firefox directory, does it also share the Firefox
        // version info? We only care about this inside the app directory.
        if (mExeVersion.isSome() &&
            (vi.mFileVersion.Version64() == mExeVersion.value())) {
          aDllInfo.mTrustFlags |= ModuleTrustFlags::FirefoxDirectoryAndVersion;
          score += 50;
        }
      }
    }
  }

  if (score < scoreThreshold) {
    if (aSvc) {
      UniquePtr<wchar_t[]> szSignedBy = aSvc->GetBinaryOrgName(dllFullPath.get());
      if (szSignedBy) {
        nsAutoString signedBy(szSignedBy.get());
        if (signedBy.EqualsLiteral("Microsoft Windows")) {
          aDllInfo.mTrustFlags |= ModuleTrustFlags::MicrosoftWindowsSignature;
          score = 100;
        } else if (signedBy.EqualsLiteral("Microsoft Corporation")) {
          aDllInfo.mTrustFlags |= ModuleTrustFlags::MicrosoftWindowsSignature;
          score = 100;
        } else if (signedBy.EqualsLiteral("Mozilla Corporation")) {
          aDllInfo.mTrustFlags |= ModuleTrustFlags::MozillaSignature;
          score = 100;
        }
      }
    }
  }

  return Some(score >= scoreThreshold);
}

} // namespace mozilla
