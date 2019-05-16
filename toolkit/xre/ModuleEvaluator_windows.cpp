/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ModuleEvaluator_windows.h"

#include <windows.h>
#include <algorithm>  // For std::find()

#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
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
static bool GetDirectoryName(const nsCOMPtr<nsIFile> aFile,
                             nsAString& aParent) {
  nsCOMPtr<nsIFile> parentDir;
  if (NS_FAILED(aFile->GetParent(getter_AddRefs(parentDir))) || !parentDir) {
    return false;
  }
  if (NS_FAILED(parentDir->GetPath(aParent))) {
    return false;
  }
  return true;
}

ModuleLoadEvent::ModuleInfo::ModuleInfo(uintptr_t aBase)
    : mBase(aBase), mTrustFlags(ModuleTrustFlags::None) {}

ModuleLoadEvent::ModuleInfo::ModuleInfo(
    const glue::ModuleLoadEvent::ModuleInfo& aOther)
    : mBase(aOther.mBase),
      mLoadDurationMS(Some(aOther.mLoadDurationMS)),
      mTrustFlags(ModuleTrustFlags::None) {
  if (aOther.mLdrName) {
    mLdrName.Assign(aOther.mLdrName.get());
  }

  if (aOther.mFullPath) {
    nsDependentString tempPath(aOther.mFullPath.get());
    DebugOnly<nsresult> rv =
        NS_NewLocalFile(tempPath, false, getter_AddRefs(mFile));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

bool ModuleLoadEvent::ModuleInfo::PopulatePathInfo() {
  MOZ_ASSERT(mBase && mLdrName.IsEmpty() && !mFile);
  if (!widget::WinUtils::GetModuleFullPath(reinterpret_cast<HMODULE>(mBase),
                                           mLdrName)) {
    return false;
  }

  return NS_SUCCEEDED(NS_NewLocalFile(mLdrName, false, getter_AddRefs(mFile)));
}

bool ModuleLoadEvent::ModuleInfo::PrepForTelemetry() {
  MOZ_ASSERT(!mLdrName.IsEmpty() && mFile);
  if (mLdrName.IsEmpty() || !mFile) {
    return false;
  }

  using PathTransformFlags = widget::WinUtils::PathTransformFlags;

  if (!widget::WinUtils::PreparePathForTelemetry(
          mLdrName,
          PathTransformFlags::Default & ~PathTransformFlags::Canonicalize)) {
    return false;
  }

  nsAutoString dllFullPath;
  if (NS_FAILED(mFile->GetPath(dllFullPath))) {
    return false;
  }

  if (!widget::WinUtils::MakeLongPath(dllFullPath)) {
    return false;
  }

  // Replace mFile with the lengthened version
  if (NS_FAILED(NS_NewLocalFile(dllFullPath, false, getter_AddRefs(mFile)))) {
    return false;
  }

  nsAutoString sanitized(dllFullPath);

  if (!widget::WinUtils::PreparePathForTelemetry(
          sanitized,
          PathTransformFlags::Default & ~(PathTransformFlags::Canonicalize |
                                          PathTransformFlags::Lengthen))) {
    return false;
  }

  mFilePathClean = std::move(sanitized);

  return true;
}

ModuleLoadEvent::ModuleLoadEvent(const ModuleLoadEvent& aOther,
                                 CopyOption aOption)
    : mIsStartup(aOther.mIsStartup),
      mThreadID(aOther.mThreadID),
      mThreadName(aOther.mThreadName),
      mProcessUptimeMS(aOther.mProcessUptimeMS) {
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
    : mIsStartup(
          false)  // Events originating in glue:: cannot be a startup event.
      ,
      mThreadID(aOther.mThreadID),
      mProcessUptimeMS(aOther.mProcessUptimeMS) {
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
static void GetKeyboardLayoutDlls(
    Vector<nsString, 0, InfallibleAllocPolicy>& aOut) {
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
    if (RegEnumKeyExW(rawKey, iKey, strTemp, &strTempSize, nullptr, nullptr,
                      nullptr, nullptr) != ERROR_SUCCESS) {
      return;  // ERROR_NO_MORE_ITEMS or a real error: bail with what we have.
    }
    iKey++;

    strTempSize = sizeof(strTemp);
    if (::RegGetValueW(rawKey, strTemp, L"Layout File", RRF_RT_REG_SZ, nullptr,
                       strTemp, &strTempSize) == ERROR_SUCCESS) {
      nsString ws(strTemp, (strTempSize / sizeof(wchar_t)) - 1);
      ToLowerCase(ws);  // To facilitate searches
      Unused << aOut.emplaceBack(ws);
    }
  }
}

ModuleEvaluator::ModuleEvaluator() {
  GetKeyboardLayoutDlls(mKeyboardLayoutDlls);

  nsresult rv =
      NS_GetSpecialDirectory(NS_OS_SYSTEM_DIR, getter_AddRefs(mSysDirectory));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIFile> winSxSDir;
  rv = NS_GetSpecialDirectory(NS_WIN_WINDOWS_DIR, getter_AddRefs(winSxSDir));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_SUCCEEDED(rv)) {
    rv = winSxSDir->Append(NS_LITERAL_STRING("WinSxS"));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv)) {
      mWinSxSDirectory = std::move(winSxSDir);
    }
  }

#ifdef _M_IX86
  WCHAR sysWow64Buf[MAX_PATH + 1] = {};

  UINT sysWowLen =
      ::GetSystemWow64DirectoryW(sysWow64Buf, ArrayLength(sysWow64Buf));
  if (sysWowLen > 0 && sysWowLen < ArrayLength(sysWow64Buf)) {
    rv = NS_NewLocalFile(nsDependentString(sysWow64Buf, sysWowLen), false,
                         getter_AddRefs(mSysWOW64Directory));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
#endif  // _M_IX86

  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(mExeDirectory));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIFile> exeFile;
  rv = XRE_GetBinaryPath(getter_AddRefs(exeFile));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_SUCCEEDED(rv)) {
    nsAutoString exePath;
    rv = exeFile->GetPath(exePath);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv)) {
      ModuleVersionInfo exeVi;
      if (exeVi.GetFromImage(exePath)) {
        mExeVersion = Some(exeVi.mFileVersion.Version64());
      }
    }
  }
}

Maybe<bool> ModuleEvaluator::IsModuleTrusted(
    ModuleLoadEvent::ModuleInfo& aDllInfo, const ModuleLoadEvent& aEvent,
    Authenticode* aSvc) const {
  MOZ_ASSERT(aDllInfo.mTrustFlags == ModuleTrustFlags::None);
  MOZ_ASSERT(aDllInfo.mFile);

  nsAutoString dllFullPath;
  if (NS_FAILED(aDllInfo.mFile->GetPath(dllFullPath))) {
    return Nothing();
  }

  // We start by checking authenticode signatures, since any result from this
  // test will produce an immediate pass/fail.
  if (aSvc) {
    UniquePtr<wchar_t[]> szSignedBy = aSvc->GetBinaryOrgName(dllFullPath.get());

    if (szSignedBy) {
      nsDependentString signedBy(szSignedBy.get());

      if (signedBy.EqualsLiteral("Microsoft Windows")) {
        aDllInfo.mTrustFlags |= ModuleTrustFlags::MicrosoftWindowsSignature;
        return Some(true);
      } else if (signedBy.EqualsLiteral("Microsoft Corporation")) {
        aDllInfo.mTrustFlags |= ModuleTrustFlags::MicrosoftWindowsSignature;
        return Some(true);
      } else if (signedBy.EqualsLiteral("Mozilla Corporation")) {
        aDllInfo.mTrustFlags |= ModuleTrustFlags::MozillaSignature;
        return Some(true);
      } else {
        // Being signed by somebody who is neither Microsoft nor us is an
        // automatic disqualification.
        aDllInfo.mTrustFlags = ModuleTrustFlags::None;
        return Some(false);
      }
    }
  }

  nsAutoString dllDirectory;
  if (!GetDirectoryName(aDllInfo.mFile, dllDirectory)) {
    return Nothing();
  }

  nsAutoString dllLeafLower;
  if (NS_FAILED(aDllInfo.mFile->GetLeafName(dllLeafLower))) {
    return Nothing();
  }

  ToLowerCase(dllLeafLower);  // To facilitate case-insensitive searching

  // The JIT profiling module doesn't really have any other practical way to
  // match; hard-code it as being trusted.
  if (dllLeafLower.EqualsLiteral("jitpi.dll")) {
    aDllInfo.mTrustFlags = ModuleTrustFlags::JitPI;
    return Some(true);
  }

  // Accumulate a trustworthiness score as the module passes through several
  // checks. If the score ever reaches above the threshold, it's considered
  // trusted.
  uint32_t scoreThreshold = 100;

#ifdef ENABLE_TESTS
  // Check whether we are running as an xpcshell test.
  if (mozilla::EnvHasValue("XPCSHELL_TEST_PROFILE_DIR")) {
    // During xpcshell tests, these DLLs are hard-coded to pass through all
    // criteria checks and still result in "untrusted" status, so they show up
    // in the untrusted modules ping for the test to examine.
    // Setting the threshold very high ensures the test will cover all criteria.
    if (dllLeafLower.EqualsLiteral("untrusted-startup-test-dll.dll") ||
        dllLeafLower.EqualsLiteral("modules-test.dll")) {
      scoreThreshold = 99999;
    }
  }
#endif

  nsresult rv;
  bool contained;

  uint32_t score = 0;

  if (score < scoreThreshold) {
    // Is the DLL in the system directory?
    rv = mSysDirectory->Contains(aDllInfo.mFile, &contained);
    if (NS_SUCCEEDED(rv) && contained) {
      aDllInfo.mTrustFlags |= ModuleTrustFlags::SystemDirectory;
      score += 50;
    }
  }

#ifdef _M_IX86
  // Under WOW64, SysWOW64 is the effective system directory. Give SysWOW64 the
  // same trustworthiness as ModuleTrustFlags::SystemDirectory.
  if (mSysWOW64Directory) {
    rv = mSysWOW64Directory->Contains(aDllInfo.mFile, &contained);
    if (NS_SUCCEEDED(rv) && contained) {
      aDllInfo.mTrustFlags |= ModuleTrustFlags::SysWOW64Directory;
      score += 50;
    }
  }
#endif  // _M_IX86

  // Is the DLL in the WinSxS directory? Some Microsoft DLLs (e.g. comctl32) are
  // loaded from here and don't have digital signatures. So while this is not a
  // guarantee of trustworthiness, but is at least as valid as system32.
  rv = mWinSxSDirectory->Contains(aDllInfo.mFile, &contained);
  if (NS_SUCCEEDED(rv) && contained) {
    aDllInfo.mTrustFlags |= ModuleTrustFlags::WinSxSDirectory;
    score += 50;
  }

  // Is it a keyboard layout DLL?
  if (std::find(mKeyboardLayoutDlls.begin(), mKeyboardLayoutDlls.end(),
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

      rv = mExeDirectory->Contains(aDllInfo.mFile, &contained);
      if (NS_SUCCEEDED(rv) && contained) {
        score += 50;
        aDllInfo.mTrustFlags |= ModuleTrustFlags::FirefoxDirectory;

        if (dllLeafLower.EqualsLiteral("xul.dll")) {
          // The caller wants to know if this DLL is xul.dll, but this flag
          // doesn't need to affect trust score. Xul will be considered trusted
          // by other measures.
          aDllInfo.mTrustFlags |= ModuleTrustFlags::Xul;
        }

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

  return Some(score >= scoreThreshold);
}

}  // namespace mozilla
