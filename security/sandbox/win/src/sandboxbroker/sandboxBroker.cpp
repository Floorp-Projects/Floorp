/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "sandboxBroker.h"

#include <aclapi.h>
#include <shlobj.h>
#include <string>

#include "base/win/windows_version.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ImportDir.h"
#include "mozilla/Logging.h"
#include "mozilla/NSPRLogModulesParser.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Preferences.h"
#include "mozilla/SandboxSettings.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WinDllServices.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/ipc/LaunchError.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIXULRuntime.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "sandbox/win/src/app_container_profile.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"
#include "WinUtils.h"

#define SANDBOX_SUCCEED_OR_CRASH(x)                                   \
  do {                                                                \
    sandbox::ResultCode result = (x);                                 \
    MOZ_RELEASE_ASSERT(result == sandbox::SBOX_ALL_OK, #x " failed"); \
  } while (0)

namespace mozilla {

constexpr wchar_t kLpacFirefoxInstallFiles[] = L"lpacFirefoxInstallFiles";

sandbox::BrokerServices* sBrokerService = nullptr;

// This is set to true in Initialize when our exe file name has a drive type of
// DRIVE_REMOTE, so that we can tailor the sandbox policy as some settings break
// fundamental things when running from a network drive. We default to false in
// case those checks fail as that gives us the strongest policy.
bool SandboxBroker::sRunningFromNetworkDrive = false;

// Cached special directories used for adding policy rules.
static StaticAutoPtr<nsString> sBinDir;
static StaticAutoPtr<nsString> sProfileDir;
static StaticAutoPtr<nsString> sLocalAppDataDir;
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
static StaticAutoPtr<nsString> sUserExtensionsDir;
#endif

static LazyLogModule sSandboxBrokerLog("SandboxBroker");

#define LOG_E(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Error, (__VA_ARGS__))
#define LOG_W(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Warning, (__VA_ARGS__))
#define LOG_D(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Debug, (__VA_ARGS__))

// Used to store whether we have accumulated an error combination for this
// session.
static StaticAutoPtr<nsTHashtable<nsCStringHashKey>> sLaunchErrors;

// This helper function is our version of SandboxWin::AddWin32kLockdownPolicy
// of Chromium, making sure the MITIGATION_WIN32K_DISABLE flag is set before
// adding the SUBSYS_WIN32K_LOCKDOWN rule which is required by
// PolicyBase::AddRuleInternal.
static sandbox::ResultCode AddWin32kLockdownPolicy(
    sandbox::TargetPolicy* aPolicy, bool aEnableOpm) {
  sandbox::MitigationFlags flags = aPolicy->GetProcessMitigations();
  MOZ_ASSERT(flags,
             "Mitigations should be set before AddWin32kLockdownPolicy.");
  MOZ_ASSERT(!(flags & sandbox::MITIGATION_WIN32K_DISABLE),
             "Check not enabling twice.  Should not happen.");

  flags |= sandbox::MITIGATION_WIN32K_DISABLE;
  sandbox::ResultCode result = aPolicy->SetProcessMitigations(flags);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  result =
      aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                       aEnableOpm ? sandbox::TargetPolicy::IMPLEMENT_OPM_APIS
                                  : sandbox::TargetPolicy::FAKE_USER_GDI_INIT,
                       nullptr);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }
  if (aEnableOpm) {
    aPolicy->SetEnableOPMRedirection();
  }

  return result;
}

/* static */
void SandboxBroker::Initialize(sandbox::BrokerServices* aBrokerServices) {
  sBrokerService = aBrokerServices;

  sRunningFromNetworkDrive = widget::WinUtils::RunningFromANetworkDrive();
}

static void CacheDirAndAutoClear(nsIProperties* aDirSvc, const char* aDirKey,
                                 StaticAutoPtr<nsString>* cacheVar) {
  nsCOMPtr<nsIFile> dirToCache;
  nsresult rv =
      aDirSvc->Get(aDirKey, NS_GET_IID(nsIFile), getter_AddRefs(dirToCache));
  if (NS_FAILED(rv)) {
    // This can only be an NS_WARNING, because it can fail for xpcshell tests.
    NS_WARNING("Failed to get directory to cache.");
    LOG_E("Failed to get directory to cache, key: %s.", aDirKey);
    return;
  }

  *cacheVar = new nsString();
  ClearOnShutdown(cacheVar);
  MOZ_ALWAYS_SUCCEEDS(dirToCache->GetPath(**cacheVar));

  // Convert network share path to format for sandbox policy.
  if (Substring(**cacheVar, 0, 2).Equals(u"\\\\"_ns)) {
    (*cacheVar)->InsertLiteral(u"??\\UNC", 1);
  }
}

/* static */
void SandboxBroker::GeckoDependentInitialize() {
  MOZ_ASSERT(NS_IsMainThread());

  bool haveXPCOM = XRE_GetProcessType() != GeckoProcessType_RemoteSandboxBroker;
  if (haveXPCOM) {
    // Cache directory paths for use in policy rules, because the directory
    // service must be called on the main thread.
    nsresult rv;
    nsCOMPtr<nsIProperties> dirSvc =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false,
                 "Failed to get directory service, cannot cache directories "
                 "for rules.");
      LOG_E(
          "Failed to get directory service, cannot cache directories for "
          "rules.");
      return;
    }

    CacheDirAndAutoClear(dirSvc, NS_GRE_DIR, &sBinDir);
    CacheDirAndAutoClear(dirSvc, NS_APP_USER_PROFILE_50_DIR, &sProfileDir);
    CacheDirAndAutoClear(dirSvc, NS_WIN_LOCAL_APPDATA_DIR, &sLocalAppDataDir);
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    CacheDirAndAutoClear(dirSvc, XRE_USER_SYS_EXTENSION_DIR,
                         &sUserExtensionsDir);
#endif
  }

  // Create sLaunchErrors up front because ClearOnShutdown must be called on the
  // main thread.
  sLaunchErrors = new nsTHashtable<nsCStringHashKey>();
  ClearOnShutdown(&sLaunchErrors);
}

SandboxBroker::SandboxBroker() {
  if (sBrokerService) {
    scoped_refptr<sandbox::TargetPolicy> policy =
        sBrokerService->CreatePolicy();
    mPolicy = policy.get();
    mPolicy->AddRef();
    if (sRunningFromNetworkDrive) {
      mPolicy->SetDoNotUseRestrictingSIDs();
    }
  } else {
    mPolicy = nullptr;
  }
}

#define WSTRING(STRING) L"" STRING

static void AddMozLogRulesToPolicy(sandbox::TargetPolicy* aPolicy,
                                   const base::EnvironmentMap& aEnvironment) {
  auto it = aEnvironment.find(ENVIRONMENT_LITERAL("MOZ_LOG_FILE"));
  if (it == aEnvironment.end()) {
    it = aEnvironment.find(ENVIRONMENT_LITERAL("NSPR_LOG_FILE"));
  }
  if (it == aEnvironment.end()) {
    return;
  }

  char const* logFileModules = getenv("MOZ_LOG");
  if (!logFileModules) {
    return;
  }

  // MOZ_LOG files have a standard file extension appended.
  std::wstring logFileName(it->second);
  logFileName.append(WSTRING(MOZ_LOG_FILE_EXTENSION));

  // Allow for rotation number if rotate is on in the MOZ_LOG settings.
  bool rotate = false;
  NSPRLogModulesParser(
      logFileModules,
      [&rotate](const char* aName, LogLevel aLevel, int32_t aValue) {
        if (strcmp(aName, "rotate") == 0) {
          // Less or eq zero means to turn rotate off.
          rotate = aValue > 0;
        }
      });
  if (rotate) {
    logFileName.append(L".?");
  }

  // Allow for %PID token in the filename. We don't allow it in the dir path, if
  // specified, because we have to use a wildcard as we don't know the PID yet.
  auto pidPos = logFileName.find(WSTRING(MOZ_LOG_PID_TOKEN));
  auto lastSlash = logFileName.find_last_of(L"/\\");
  if (pidPos != std::wstring::npos &&
      (lastSlash == std::wstring::npos || lastSlash < pidPos)) {
    logFileName.replace(pidPos, strlen(MOZ_LOG_PID_TOKEN), L"*");
  }

  aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                   sandbox::TargetPolicy::FILES_ALLOW_ANY, logFileName.c_str());
}

static void AddDeveloperRepoDirToPolicy(sandbox::TargetPolicy* aPolicy) {
  const wchar_t* developer_repo_dir =
      _wgetenv(WSTRING("MOZ_DEVELOPER_REPO_DIR"));
  if (!developer_repo_dir) {
    return;
  }

  std::wstring repoPath(developer_repo_dir);
  std::replace(repoPath.begin(), repoPath.end(), '/', '\\');
  repoPath.append(WSTRING("\\*"));

  aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                   sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                   repoPath.c_str());
}

#undef WSTRING

Result<Ok, mozilla::ipc::LaunchError> SandboxBroker::LaunchApp(
    const wchar_t* aPath, const wchar_t* aArguments,
    base::EnvironmentMap& aEnvironment, GeckoProcessType aProcessType,
    const bool aEnableLogging, const IMAGE_THUNK_DATA* aCachedNtdllThunk,
    void** aProcessHandle) {
  if (!sBrokerService) {
    return Err(mozilla::ipc::LaunchError("SB::LA::sBrokerService"));
  }

  if (!mPolicy) {
    return Err(mozilla::ipc::LaunchError("SB::LA::mPolicy"));
  }

  // Set stdout and stderr, to allow inheritance for logging.
  mPolicy->SetStdoutHandle(::GetStdHandle(STD_OUTPUT_HANDLE));
  mPolicy->SetStderrHandle(::GetStdHandle(STD_ERROR_HANDLE));

  // If we're running from a network drive then we can't block loading from
  // remote locations. Strangely using MITIGATION_IMAGE_LOAD_NO_LOW_LABEL in
  // this situation also means the process fails to start (bug 1423296).
  if (sRunningFromNetworkDrive) {
    sandbox::MitigationFlags mitigations = mPolicy->GetProcessMitigations();
    mitigations &= ~(sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
                     sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL);
    MOZ_RELEASE_ASSERT(
        mPolicy->SetProcessMitigations(mitigations) == sandbox::SBOX_ALL_OK,
        "Setting the reduced set of flags should always succeed");
  }

  // If logging enabled, set up the policy.
  if (aEnableLogging) {
    ApplyLoggingPolicy();
  }

#if defined(DEBUG)
  // Allow write access to TEMP directory in debug builds for logging purposes.
  // The path from GetTempPathW can have a length up to MAX_PATH + 1, including
  // the null, so we need MAX_PATH + 2, so we can add an * to the end.
  wchar_t tempPath[MAX_PATH + 2];
  uint32_t pathLen = ::GetTempPathW(MAX_PATH + 1, tempPath);
  if (pathLen > 0) {
    // GetTempPath path ends with \ and returns the length without the null.
    tempPath[pathLen] = L'*';
    tempPath[pathLen + 1] = L'\0';
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_ANY, tempPath);
  }
#endif

  // Enable the child process to write log files when setup
  AddMozLogRulesToPolicy(mPolicy, aEnvironment);

  if (!mozilla::IsPackagedBuild()) {
    AddDeveloperRepoDirToPolicy(mPolicy);
  }

  // Create the sandboxed process
  PROCESS_INFORMATION targetInfo = {0};
  sandbox::ResultCode result;
  sandbox::ResultCode last_warning = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  result = sBrokerService->SpawnTarget(aPath, aArguments, aEnvironment, mPolicy,
                                       &last_warning, &last_error, &targetInfo);
  if (sandbox::SBOX_ALL_OK != result) {
    nsAutoCString key;
    key.AppendASCII(XRE_GeckoProcessTypeToString(aProcessType));
    key.AppendLiteral("/0x");
    key.AppendInt(static_cast<uint32_t>(last_error), 16);

    // Only accumulate for each combination once per session.
    if (sLaunchErrors) {
      if (!sLaunchErrors->Contains(key)) {
        Telemetry::Accumulate(Telemetry::SANDBOX_FAILED_LAUNCH_KEYED, key,
                              result);
        sLaunchErrors->PutEntry(key);
      }
    } else {
      // If sLaunchErrors not created yet then always accumulate.
      Telemetry::Accumulate(Telemetry::SANDBOX_FAILED_LAUNCH_KEYED, key,
                            result);
    }

    LOG_E(
        "Failed (ResultCode %d) to SpawnTarget with last_error=%lu, "
        "last_warning=%d",
        result, last_error, last_warning);

    return Err(mozilla::ipc::LaunchError("SB::LA::SpawnTarget", last_error));
  } else if (sandbox::SBOX_ALL_OK != last_warning) {
    // If there was a warning (but the result was still ok), log it and proceed.
    LOG_W("Warning on SpawnTarget with last_error=%lu, last_warning=%d",
          last_error, last_warning);
  }

#ifdef MOZ_THUNDERBIRD
  // In Thunderbird, mInitDllBlocklistOOP is null, so InitDllBlocklistOOP would
  // hit MOZ_RELEASE_ASSERT.
  constexpr bool isThunderbird = true;
#else
  constexpr bool isThunderbird = false;
#endif

  if (!isThunderbird &&
      XRE_GetChildProcBinPathType(aProcessType) == BinPathType::Self) {
    RefPtr<DllServices> dllSvc(DllServices::Get());
    LauncherVoidResultWithLineInfo blocklistInitOk =
        dllSvc->InitDllBlocklistOOP(aPath, targetInfo.hProcess,
                                    aCachedNtdllThunk, aProcessType);
    if (blocklistInitOk.isErr()) {
      dllSvc->HandleLauncherError(blocklistInitOk.unwrapErr(),
                                  XRE_GeckoProcessTypeToString(aProcessType));
      LOG_E("InitDllBlocklistOOP failed at %s:%d with HRESULT 0x%08lX",
            blocklistInitOk.unwrapErr().mFile,
            blocklistInitOk.unwrapErr().mLine,
            blocklistInitOk.unwrapErr().mError.AsHResult());
      TerminateProcess(targetInfo.hProcess, 1);
      CloseHandle(targetInfo.hThread);
      CloseHandle(targetInfo.hProcess);
      return Err(mozilla::ipc::LaunchError(
          "InitDllBlocklistOOP",
          blocklistInitOk.unwrapErr().mError.AsHResult()));
    }
  } else {
    // Load the child executable as a datafile so that we can examine its
    // headers without doing a full load with dependencies and such.
    nsModuleHandle moduleHandle(
        ::LoadLibraryExW(aPath, nullptr, LOAD_LIBRARY_AS_DATAFILE));
    if (moduleHandle) {
      nt::CrossExecTransferManager transferMgr(targetInfo.hProcess,
                                               moduleHandle);
      if (!!transferMgr) {
        LauncherVoidResult importsRestored =
            RestoreImportDirectory(aPath, transferMgr);
        if (importsRestored.isErr()) {
          RefPtr<DllServices> dllSvc(DllServices::Get());
          dllSvc->HandleLauncherError(
              importsRestored.unwrapErr(),
              XRE_GeckoProcessTypeToString(aProcessType));
          LOG_E("Failed to restore import directory with HRESULT 0x%08lX",
                importsRestored.unwrapErr().mError.AsHResult());
          TerminateProcess(targetInfo.hProcess, 1);
          CloseHandle(targetInfo.hThread);
          CloseHandle(targetInfo.hProcess);
          return Err(mozilla::ipc::LaunchError(
              "RestoreImportDirectory",
              importsRestored.unwrapErr().mError.AsHResult()));
        }
      }
    }
  }

  // The sandboxed process is started in a suspended state, resume it now that
  // we've set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  return Ok();
}

static void AddCachedDirRule(sandbox::TargetPolicy* aPolicy,
                             sandbox::TargetPolicy::Semantics aAccess,
                             const StaticAutoPtr<nsString>& aBaseDir,
                             const nsLiteralString& aRelativePath) {
  if (!aBaseDir) {
    // This can only be an NS_WARNING, because it can null for xpcshell tests.
    NS_WARNING("Tried to add rule with null base dir.");
    LOG_E("Tried to add rule with null base dir. Relative path: %S, Access: %d",
          static_cast<const wchar_t*>(aRelativePath.get()), aAccess);
    return;
  }

  nsAutoString rulePath(*aBaseDir);
  rulePath.Append(aRelativePath);

  sandbox::ResultCode result = aPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_FILES, aAccess, rulePath.get());
  if (sandbox::SBOX_ALL_OK != result) {
    NS_ERROR("Failed to add file policy rule.");
    LOG_E("Failed (ResultCode %d) to add %d access to: %S", result, aAccess,
          static_cast<const wchar_t*>(rulePath.get()));
  }
}

// This function caches and returns an array of NT paths of the executable's
// dependent modules.
// If this returns Nothing(), it means the retrieval of the modules failed
// (e.g. when the launcher process is disabled), so the process should not
// enable pre-spawn CIG.
static const Maybe<Vector<const wchar_t*>>& GetPrespawnCigExceptionModules() {
  // We enable pre-spawn CIG only in Nightly for now
  // because it caused a compat issue (bug 1682304 and 1704373).
#if defined(NIGHTLY_BUILD)
  // The shared section contains a list of dependent modules as a
  // null-delimited string.  We convert it to a string vector and
  // cache it to avoid converting the same data every time.
  static Maybe<Vector<const wchar_t*>> sDependentModules =
      []() -> Maybe<Vector<const wchar_t*>> {
    RefPtr<DllServices> dllSvc(DllServices::Get());
    auto sharedSection = dllSvc->GetSharedSection();
    if (!sharedSection) {
      return Nothing();
    }

    return sharedSection->GetDependentModules();
  }();

  return sDependentModules;
#else
  static const Maybe<Vector<const wchar_t*>> sNothing = Nothing();
  return sNothing;
#endif
}

static sandbox::ResultCode AllowProxyLoadFromBinDir(
    sandbox::TargetPolicy* aPolicy) {
  // Allow modules in the directory containing the executable such as
  // mozglue.dll, nss3.dll, etc.
  static UniquePtr<nsString> sInstallDir;
  if (!sInstallDir) {
    // Since this function can be called before sBinDir is initialized,
    // we cache the install path by ourselves.
    UniquePtr<wchar_t[]> appDirStr;
    if (GetInstallDirectory(appDirStr)) {
      sInstallDir = MakeUnique<nsString>(appDirStr.get());
      sInstallDir->Append(u"\\*");

      auto setClearOnShutdown = [ptr = &sInstallDir]() -> void {
        ClearOnShutdown(ptr);
      };
      if (NS_IsMainThread()) {
        setClearOnShutdown();
      } else {
        SchedulerGroup::Dispatch(NS_NewRunnableFunction(
            "InitSignedPolicyRulesToBypassCig", std::move(setClearOnShutdown)));
      }
    }

    if (!sInstallDir) {
      return sandbox::SBOX_ERROR_GENERIC;
    }
  }
  return aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                          sandbox::TargetPolicy::SIGNED_ALLOW_LOAD,
                          sInstallDir->get());
}

static sandbox::ResultCode AddCigToPolicy(
    sandbox::TargetPolicy* aPolicy, bool aAlwaysProxyBinDirLoading = false) {
  const Maybe<Vector<const wchar_t*>>& exceptionModules =
      GetPrespawnCigExceptionModules();
  if (exceptionModules.isNothing()) {
    sandbox::MitigationFlags delayedMitigations =
        aPolicy->GetDelayedProcessMitigations();
    MOZ_ASSERT(delayedMitigations,
               "Delayed mitigations should be set before AddCigToPolicy.");
    MOZ_ASSERT(!(delayedMitigations & sandbox::MITIGATION_FORCE_MS_SIGNED_BINS),
               "AddCigToPolicy should not be called twice.");

    delayedMitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
    sandbox::ResultCode result =
        aPolicy->SetDelayedProcessMitigations(delayedMitigations);
    if (result != sandbox::SBOX_ALL_OK) {
      return result;
    }

    if (aAlwaysProxyBinDirLoading) {
      result = AllowProxyLoadFromBinDir(aPolicy);
    }
    return result;
  }

  sandbox::MitigationFlags mitigations = aPolicy->GetProcessMitigations();
  MOZ_ASSERT(mitigations, "Mitigations should be set before AddCigToPolicy.");
  MOZ_ASSERT(!(mitigations & sandbox::MITIGATION_FORCE_MS_SIGNED_BINS),
             "AddCigToPolicy should not be called twice.");

  mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  sandbox::ResultCode result = aPolicy->SetProcessMitigations(mitigations);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  result = AllowProxyLoadFromBinDir(aPolicy);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  for (const wchar_t* path : exceptionModules.ref()) {
    result = aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                              sandbox::TargetPolicy::SIGNED_ALLOW_LOAD, path);
    if (result != sandbox::SBOX_ALL_OK) {
      return result;
    }
  }

  return sandbox::SBOX_ALL_OK;
}

// Returns the most strict dynamic code mitigation flag that is compatible with
// system libraries MSAudDecMFT.dll and msmpeg2vdec.dll. This depends on the
// Windows version and the architecture. See bug 1783223 comment 27.
//
// Use the result with SetDelayedProcessMitigations. Using non-delayed ACG
// results in incompatibility with third-party antivirus software, the Windows
// internal Shim Engine mechanism, parts of our own DLL blocklist code, and
// AddressSanitizer initialization code. See bug 1783223.
static sandbox::MitigationFlags DynamicCodeFlagForSystemMediaLibraries() {
  static auto dynamicCodeFlag = []() {
#ifdef _M_X64
    if (IsWin10CreatorsUpdateOrLater()) {
      return sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    }
#endif  // _M_X64

    if (IsWin10AnniversaryUpdateOrLater()) {
      return sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT;
    }

    return sandbox::MitigationFlags{};
  }();
  return dynamicCodeFlag;
}

// Process fails to start in LPAC with ASan build
#if !defined(MOZ_ASAN)
static void HexEncode(const Span<const uint8_t>& aBytes, nsACString& aEncoded) {
  static const char kHexChars[] = "0123456789abcdef";

  // Each input byte creates two output hex characters.
  char* encodedPtr;
  aEncoded.GetMutableData(&encodedPtr, aBytes.size() * 2);

  for (auto byte : aBytes) {
    *(encodedPtr++) = kHexChars[byte >> 4];
    *(encodedPtr++) = kHexChars[byte & 0xf];
  }
}

// This is left as a void because we might fail to set the permission for some
// reason and yet the LPAC permission is already granted. So returning success
// or failure isn't really that useful.
/* static */
void SandboxBroker::EnsureLpacPermsissionsOnDir(const nsString& aDir) {
  // For MSIX packages we get access through the packageContents capability and
  // we probably won't have access to add the permission either way.
  if (widget::WinUtils::HasPackageIdentity()) {
    return;
  }

  BYTE sidBytes[SECURITY_MAX_SID_SIZE];
  PSID lpacFirefoxInstallFilesSid = static_cast<PSID>(sidBytes);
  if (!sBrokerService->DeriveCapabilitySidFromName(kLpacFirefoxInstallFiles,
                                                   lpacFirefoxInstallFilesSid,
                                                   sizeof(sidBytes))) {
    LOG_E("Failed to derive Firefox install files capability SID.");
    return;
  }

  HANDLE hDir = ::CreateFileW(aDir.get(), WRITE_DAC | READ_CONTROL, 0, NULL,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hDir == INVALID_HANDLE_VALUE) {
    LOG_W("Unable to get directory handle for %s",
          NS_ConvertUTF16toUTF8(aDir).get());
    return;
  }

  UniquePtr<HANDLE, CloseHandleDeleter> autoHandleCloser(hDir);
  PACL pBinDirAcl = nullptr;
  PSECURITY_DESCRIPTOR pSD = nullptr;
  DWORD result =
      ::GetSecurityInfo(hDir, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, &pBinDirAcl, nullptr, &pSD);
  if (result != ERROR_SUCCESS) {
    LOG_E("Failed to get DACL for %s", NS_ConvertUTF16toUTF8(aDir).get());
    return;
  }

  UniquePtr<VOID, LocalFreeDeleter> autoFreeSecDesc(pSD);
  if (!pBinDirAcl) {
    LOG_E("DACL was null for %s", NS_ConvertUTF16toUTF8(aDir).get());
    return;
  }

  for (DWORD i = 0; i < pBinDirAcl->AceCount; ++i) {
    VOID* pAce = nullptr;
    if (!::GetAce(pBinDirAcl, i, &pAce) ||
        static_cast<PACE_HEADER>(pAce)->AceType != ACCESS_ALLOWED_ACE_TYPE) {
      continue;
    }

    auto* pAllowedAce = static_cast<ACCESS_ALLOWED_ACE*>(pAce);
    if ((pAllowedAce->Mask & (GENERIC_READ | GENERIC_EXECUTE)) !=
        (GENERIC_READ | GENERIC_EXECUTE)) {
      continue;
    }

    PSID aceSID = reinterpret_cast<PSID>(&(pAllowedAce->SidStart));
    if (::EqualSid(aceSID, lpacFirefoxInstallFilesSid)) {
      LOG_D("Firefox install files permission found on %s",
            NS_ConvertUTF16toUTF8(aDir).get());
      return;
    }
  }

  EXPLICIT_ACCESS_W newAccess = {0};
  newAccess.grfAccessMode = GRANT_ACCESS;
  newAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
  newAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  ::BuildTrusteeWithSidW(&newAccess.Trustee, lpacFirefoxInstallFilesSid);
  PACL newDacl = nullptr;
  if (ERROR_SUCCESS !=
      ::SetEntriesInAclW(1, &newAccess, pBinDirAcl, &newDacl)) {
    LOG_E("Failed to create new DACL with Firefox install files SID.");
    return;
  }

  UniquePtr<ACL, LocalFreeDeleter> autoFreeAcl(newDacl);
  if (ERROR_SUCCESS != ::SetSecurityInfo(hDir, SE_FILE_OBJECT,
                                         DACL_SECURITY_INFORMATION, nullptr,
                                         nullptr, newDacl, nullptr)) {
    LOG_E("Failed to set new DACL on %s", NS_ConvertUTF16toUTF8(aDir).get());
  }

  LOG_D("Firefox install files permission granted on %s",
        NS_ConvertUTF16toUTF8(aDir).get());
}

static bool IsLowPrivilegedAppContainerSupported() {
  // Chromium doesn't support adding an LPAC before this version due to
  // incompatibility with some process mitigations.
  return IsWin10Sep2018UpdateOrLater();
}

// AddAndConfigureAppContainerProfile deliberately fails if it is called on an
// unsupported version. This is because for some process types the LPAC is
// required to provide a sufficiently strong sandbox. Processes where the use of
// an LPAC is an optional extra should use IsLowPrivilegedAppContainerSupported
// to check support first.
static sandbox::ResultCode AddAndConfigureAppContainerProfile(
    sandbox::TargetPolicy* aPolicy, const nsAString& aPackagePrefix,
    const nsTArray<sandbox::WellKnownCapabilities>& aWellKnownCapabilites,
    const nsTArray<const wchar_t*>& aNamedCapabilites) {
  // CreateAppContainerProfile requires that the profile name is at most 64
  // characters but 50 on WCOS systems. The size of sha1 is a constant 40,
  // so validate that the base names are sufficiently short that the total
  // length is valid on all systems.
  MOZ_ASSERT(aPackagePrefix.Length() <= 10U,
             "AppContainer Package prefix too long.");

  if (!IsLowPrivilegedAppContainerSupported()) {
    return sandbox::SBOX_ERROR_UNSUPPORTED;
  }

  static nsAutoString uniquePackageStr = []() {
    // userenv.dll may not have been loaded and some of the chromium sandbox
    // AppContainer code assumes that it is. Done here to load once.
    ::LoadLibraryW(L"userenv.dll");

    // Done during the package string initialization so we only do it once.
    SandboxBroker::EnsureLpacPermsissionsOnDir(*sBinDir.get());

    // This mirrors Edge's use of the exe path for the SHA1 hash to give a
    // machine unique name per install.
    nsAutoString ret;
    char exePathBuf[MAX_PATH];
    DWORD pathSize = ::GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    if (!pathSize) {
      return ret;
    }

    SHA1Sum sha1Sum;
    SHA1Sum::Hash sha1Hash;
    sha1Sum.update(exePathBuf, pathSize);
    sha1Sum.finish(sha1Hash);

    nsAutoCString hexEncoded;
    HexEncode(sha1Hash, hexEncoded);
    ret = NS_ConvertUTF8toUTF16(hexEncoded);
    return ret;
  }();

  if (uniquePackageStr.IsEmpty()) {
    return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE;
  }

  // The bool parameter is called create_profile, but in fact it tries to create
  // and then opens if it already exists. So always passing true is fine.
  bool createOrOpenProfile = true;
  nsAutoString packageName = aPackagePrefix + uniquePackageStr;
  sandbox::ResultCode result =
      aPolicy->AddAppContainerProfile(packageName.get(), createOrOpenProfile);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  // This looks odd, but unfortunately holding a scoped_refptr and
  // dereferencing has DCHECKs that cause a linking problem.
  sandbox::AppContainerProfile* profile =
      aPolicy->GetAppContainerProfile().get();
  profile->SetEnableLowPrivilegeAppContainer(true);

  for (auto wkCap : aWellKnownCapabilites) {
    if (!profile->AddCapability(wkCap)) {
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
    }
  }

  for (auto namedCap : aNamedCapabilites) {
    if (!profile->AddCapability(namedCap)) {
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
    }
  }

  return sandbox::SBOX_ALL_OK;
}
#endif

void SandboxBroker::SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                                      bool aIsFileProcess) {
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
  MOZ_RELEASE_ASSERT(aSandboxLevel >= 1,
                     "Should not be called with aSandboxLevel < 1");
  if (aSandboxLevel >= 20) {
    jobLevel = sandbox::JOB_LOCKDOWN;
    accessTokenLevel = sandbox::USER_LOCKDOWN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_UNTRUSTED;
  } else if (aSandboxLevel >= 4) {
    jobLevel = sandbox::JOB_LOCKDOWN;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel >= 3) {
    jobLevel = sandbox::JOB_RESTRICTED;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel == 2) {
    jobLevel = sandbox::JOB_INTERACTIVE;
    accessTokenLevel = sandbox::USER_INTERACTIVE;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else {
    MOZ_ASSERT(aSandboxLevel == 1);

    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  }

  // If the process will handle file: URLs, don't allow settings that
  // block reads.
  if (aIsFileProcess) {
    if (accessTokenLevel < sandbox::USER_NON_ADMIN) {
      accessTokenLevel = sandbox::USER_NON_ADMIN;
    }
    if (delayedIntegrityLevel > sandbox::INTEGRITY_LEVEL_LOW) {
      delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    }
  }

#if defined(DEBUG)
  // This is required for a MOZ_ASSERT check in WindowsMessageLoop.cpp
  // WinEventHook, see bug 1366694 for details.
  DWORD uiExceptions = JOB_OBJECT_UILIMIT_HANDLES;
#else
  DWORD uiExceptions = 0;
#endif
  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel, uiExceptions);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when "
                     "jobLevel == JOB_NONE?");

  // If the delayed access token is not restricted we don't want the initial one
  // to be either, because it can interfere with running from a network drive.
  sandbox::TokenLevel initialAccessTokenLevel =
      (accessTokenLevel == sandbox::USER_UNPROTECTED ||
       accessTokenLevel == sandbox::USER_NON_ADMIN)
          ? sandbox::USER_UNPROTECTED
          : sandbox::USER_RESTRICTED_SAME_ACCESS;

  result = mPolicy->SetTokenLevel(initialAccessTokenLevel, accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST "
                     "if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "SetDelayedIntegrityLevel should never fail, what happened?");

  if (aSandboxLevel > 5) {
    mPolicy->SetLockdownDefaultDacl();
    mPolicy->AddRestrictingRandomSid();
  }

  if (aSandboxLevel > 4) {
    // Alternate winstation breaks native theming.
    bool useAlternateWinstation =
        StaticPrefs::widget_non_native_theme_enabled();
    result = mPolicy->SetAlternateDesktop(useAlternateWinstation);
    if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
      LOG_W("SetAlternateDesktop failed, result: %i, last error: %lx", result,
            ::GetLastError());
    }
  }

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_DEP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

#if defined(_M_ARM64)
  // Disable CFG on older versions of ARM64 Windows to avoid a crash in COM.
  if (!IsWin10Sep2018UpdateOrLater()) {
    mitigations |= sandbox::MITIGATION_CONTROL_FLOW_GUARD_DISABLE;
  }
#endif

  if (StaticPrefs::security_sandbox_content_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetProcessMitigations.");

  nsIXULRuntime::ContentWin32kLockdownState win32kLockdownState =
      GetContentWin32kLockdownState();

  LOG_W("Win32k Lockdown State: '%s'",
        ContentWin32kLockdownStateToString(win32kLockdownState));

  if (GetContentWin32kLockdownEnabled()) {
    result = AddWin32kLockdownPolicy(mPolicy, false);
    MOZ_RELEASE_ASSERT(result == sandbox::SBOX_ALL_OK,
                       "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetDelayedProcessMitigations.");

  // We still have edge cases where the child at low integrity can't read some
  // files, so add a rule to allow read access to everything when required.
  if (aSandboxLevel == 1 || aIsFileProcess) {
    result =
        mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                         sandbox::TargetPolicy::FILES_ALLOW_READONLY, L"*");
    MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                       "With these static arguments AddRule should never fail, "
                       "what happened?");
  } else {
    // Add rule to allow access to user specific fonts.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sLocalAppDataDir, u"\\Microsoft\\Windows\\Fonts\\*"_ns);

    // Add rule to allow read access to installation directory.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sBinDir, u"\\*"_ns);

    // Add rule to allow read access to the chrome directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, u"\\chrome\\*"_ns);

    // Add rule to allow read access to the extensions directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, u"\\extensions\\*"_ns);

#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    // Add rule to allow read access to the per-user extensions directory.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sUserExtensionsDir, u"\\*"_ns);
#endif
  }

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate named pipes back to the
  // broker and other child processes, which are File type handles.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"File");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"File");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate semaphore handles,
  // to the broker process and other child processes.
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Semaphore");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Semaphore");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // Allow content processes to use complex line breaking brokering.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_LINE_BREAK,
                            sandbox::TargetPolicy::LINE_BREAK_ALLOW, nullptr);
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  if (aSandboxLevel >= 20) {
    // Content process still needs to be able to read fonts.
    wchar_t* fontsPath;
    if (SUCCEEDED(
            ::SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &fontsPath))) {
      std::wstring fontsStr = fontsPath;
      ::CoTaskMemFree(fontsPath);
      result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                                fontsStr.c_str());
      if (sandbox::SBOX_ALL_OK != result) {
        NS_ERROR("Failed to add fonts dir read access policy rule.");
        LOG_E("Failed (ResultCode %d) to add read access to: %S", result,
              fontsStr.c_str());
      }

      fontsStr += L"\\*";
      result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                                fontsStr.c_str());
      if (sandbox::SBOX_ALL_OK != result) {
        NS_ERROR("Failed to add fonts read access policy rule.");
        LOG_E("Failed (ResultCode %d) to add read access to: %S", result,
              fontsStr.c_str());
      }
    }

    // We still currently create IPC named pipes in the content process.
    result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                              sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\chrome.*");
    MOZ_RELEASE_ASSERT(
        sandbox::SBOX_ALL_OK == result,
        "With these static arguments AddRule should never fail.");
  }
}

void SandboxBroker::SetSecurityLevelForGPUProcess(int32_t aSandboxLevel) {
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");
  MOZ_RELEASE_ASSERT(aSandboxLevel >= 1);

  sandbox::TokenLevel initialTokenLevel = sandbox::USER_RESTRICTED_SAME_ACCESS;
  sandbox::TokenLevel lockdownTokenLevel =
      (aSandboxLevel >= 2) ? sandbox::USER_LIMITED
                           : sandbox::USER_RESTRICTED_NON_ADMIN;

  sandbox::IntegrityLevel initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  sandbox::IntegrityLevel delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;

  sandbox::JobLevel jobLevel = sandbox::JOB_LIMITED_USER;

  uint32_t uiExceptions =
      JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS | JOB_OBJECT_UILIMIT_DESKTOP |
      JOB_OBJECT_UILIMIT_EXITWINDOWS | JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;

  sandbox::MitigationFlags initialMitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL | sandbox::MITIGATION_DEP;

  if (StaticPrefs::security_sandbox_gpu_shadow_stack_enabled()) {
    initialMitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  sandbox::MitigationFlags delayedMitigations =
      sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
      sandbox::MITIGATION_DLL_SEARCH_ORDER;

  SANDBOX_SUCCEED_OR_CRASH(mPolicy->SetJobLevel(jobLevel, uiExceptions));
  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->SetTokenLevel(initialTokenLevel, lockdownTokenLevel));
  SANDBOX_SUCCEED_OR_CRASH(mPolicy->SetIntegrityLevel(initialIntegrityLevel));
  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel));
  SANDBOX_SUCCEED_OR_CRASH(mPolicy->SetProcessMitigations(initialMitigations));
  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->SetDelayedProcessMitigations(delayedMitigations));

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  SANDBOX_SUCCEED_OR_CRASH(mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_FILES,
      sandbox::TargetPolicy::FILES_ALLOW_ANY, L"\\??\\pipe\\chrome.*"));

  // Add the policy for the client side of the crash server pipe.
  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                       sandbox::TargetPolicy::FILES_ALLOW_ANY,
                       L"\\??\\pipe\\gecko-crash-server-pipe.*"));

  // The GPU process needs to write to a shader cache for performance reasons
  if (sProfileDir) {
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sProfileDir, u"\\shader-cache"_ns);

    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     sProfileDir, u"\\shader-cache\\*"_ns);
  }

  // The process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section"));

  SANDBOX_SUCCEED_OR_CRASH(
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section"));
}

#define SANDBOX_ENSURE_SUCCESS(result, message)          \
  do {                                                   \
    MOZ_ASSERT(sandbox::SBOX_ALL_OK == result, message); \
    if (sandbox::SBOX_ALL_OK != result) return false;    \
  } while (0)

bool SandboxBroker::SetSecurityLevelForRDDProcess() {
  if (!mPolicy) {
    return false;
  }

  auto result =
      mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  sandbox::USER_LIMITED);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %lx", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  result = mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetDelayedIntegrityLevel should never fail with "
                         "these arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

  if (StaticPrefs::security_sandbox_rdd_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  if (StaticPrefs::security_sandbox_rdd_acg_enabled()) {
    // The RDD process depends on msmpeg2vdec.dll.
    mitigations |= DynamicCodeFlagForSystemMediaLibraries();
  }

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  result = AddCigToPolicy(mPolicy);
  SANDBOX_ENSURE_SUCCESS(result, "Failed to initialize signed policy rules.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the content processes.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // This section is needed to avoid an assert during crash reporting code
  // when running mochitests.  The assertion is here:
  // toolkit/crashreporter/nsExceptionHandler.cpp:2041
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}

bool SandboxBroker::SetSecurityLevelForSocketProcess() {
  if (!mPolicy) {
    return false;
  }

  auto result =
      mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  sandbox::USER_LIMITED);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %lx", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  result =
      mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetDelayedIntegrityLevel should never fail with "
                         "these arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

  if (StaticPrefs::security_sandbox_socket_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  if (StaticPrefs::security_sandbox_socket_win32k_disable()) {
    result = AddWin32kLockdownPolicy(mPolicy, false);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER |
                sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  result = AddCigToPolicy(mPolicy);
  SANDBOX_ENSURE_SUCCESS(result, "Failed to initialize signed policy rules.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // This section is needed to avoid an assert during crash reporting code
  // when running mochitests.  The assertion is here:
  // toolkit/crashreporter/nsExceptionHandler.cpp:2041
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}

// A strict base sandbox for utility sandboxes to adapt.
struct UtilitySandboxProps {
  sandbox::JobLevel mJobLevel = sandbox::JOB_LOCKDOWN;

  sandbox::TokenLevel mInitialTokenLevel = sandbox::USER_RESTRICTED_SAME_ACCESS;
  sandbox::TokenLevel mDelayedTokenLevel = sandbox::USER_LOCKDOWN;

  sandbox::IntegrityLevel mInitialIntegrityLevel =  // before lockdown
      sandbox::INTEGRITY_LEVEL_LOW;
  sandbox::IntegrityLevel mDelayedIntegrityLevel =  // after lockdown
      sandbox::INTEGRITY_LEVEL_UNTRUSTED;

  bool mUseAlternateWindowStation = true;
  bool mUseAlternateDesktop = true;
  bool mLockdownDefaultDacl = true;
  bool mAddRestrictingRandomSid = true;
  bool mUseWin32kLockdown = true;
  bool mUseCig = true;

  sandbox::MitigationFlags mInitialMitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32 |
      sandbox::MITIGATION_CET_COMPAT_MODE;

  sandbox::MitigationFlags mDelayedMitigations =
      sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
      sandbox::MITIGATION_DLL_SEARCH_ORDER |
      sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;

  // Low Privileged Application Container settings;
  nsString mPackagePrefix;
  nsTArray<sandbox::WellKnownCapabilities> mWellKnownCapabilites;
  nsTArray<const wchar_t*> mNamedCapabilites;
};

struct GenericUtilitySandboxProps : public UtilitySandboxProps {};

struct UtilityAudioDecodingWmfSandboxProps : public UtilitySandboxProps {
  UtilityAudioDecodingWmfSandboxProps() {
    mDelayedTokenLevel = sandbox::USER_LIMITED;
    mDelayedMitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                          sandbox::MITIGATION_DLL_SEARCH_ORDER;
#ifdef MOZ_WMF
    if (StaticPrefs::security_sandbox_utility_wmf_acg_enabled()) {
      mDelayedMitigations |= DynamicCodeFlagForSystemMediaLibraries();
    }
#else
    mDelayedMitigations |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
#endif  // MOZ_WMF
  }
};

#ifdef MOZ_WMF_MEDIA_ENGINE
struct UtilityMfMediaEngineCdmSandboxProps : public UtilitySandboxProps {
  UtilityMfMediaEngineCdmSandboxProps() {
    mJobLevel = sandbox::JOB_INTERACTIVE;
    mInitialTokenLevel = sandbox::USER_UNPROTECTED;
    mDelayedTokenLevel = sandbox::USER_UNPROTECTED;
    mUseAlternateDesktop = false;
    mUseAlternateWindowStation = false;
    mLockdownDefaultDacl = false;
    mAddRestrictingRandomSid = false;
    mUseCig = false;

    // When we have an LPAC we can't set an integrity level and the process will
    // default to low integrity anyway. Without an LPAC using low integrity
    // causes problems with the CDMs.
    mInitialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LAST;
    mDelayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LAST;

    if (StaticPrefs::security_sandbox_utility_wmf_cdm_lpac_enabled()) {
      mPackagePrefix = u"fx.sb.cdm"_ns;
      mWellKnownCapabilites = {
          sandbox::WellKnownCapabilities::kPrivateNetworkClientServer,
          sandbox::WellKnownCapabilities::kInternetClient,
      };
      mNamedCapabilites = {
          L"lpacCom",
          L"lpacIdentityServices",
          L"lpacMedia",
          L"lpacPnPNotifications",
          L"lpacServicesManagement",
          L"lpacSessionManagement",
          L"lpacAppExperience",
          L"lpacInstrumentation",
          L"lpacCryptoServices",
          L"lpacEnterprisePolicyChangeNotifications",
          L"mediaFoundationCdmFiles",
          L"lpacMediaFoundationCdmData",
          L"registryRead",
          kLpacFirefoxInstallFiles,
          L"lpacDeviceAccess",
      };

      // For MSIX packages we need access to the package contents.
      if (widget::WinUtils::HasPackageIdentity()) {
        mNamedCapabilites.AppendElement(L"packageContents");
      }
    }
    mUseWin32kLockdown = false;
    mDelayedMitigations = sandbox::MITIGATION_DLL_SEARCH_ORDER;
  }
};
#endif

struct WindowsUtilitySandboxProps : public UtilitySandboxProps {
  WindowsUtilitySandboxProps() {
    mJobLevel = sandbox::JOB_INTERACTIVE;
    mDelayedTokenLevel = sandbox::USER_RESTRICTED_SAME_ACCESS;
    mUseAlternateWindowStation = false;
    mInitialIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
    mDelayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
    mUseWin32kLockdown = false;
    mUseCig = false;
    mDelayedMitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                          sandbox::MITIGATION_DLL_SEARCH_ORDER;
  }
};

static const char* WellKnownCapabilityNames[] = {
    "InternetClient",
    "InternetClientServer",
    "PrivateNetworkClientServer",
    "PicturesLibrary",
    "VideosLibrary",
    "MusicLibrary",
    "DocumentsLibrary",
    "EnterpriseAuthentication",
    "SharedUserCertificates",
    "RemovableStorage",
    "Appointments",
    "Contacts",
};

void LogUtilitySandboxProps(const UtilitySandboxProps& us) {
  if (!static_cast<LogModule*>(sSandboxBrokerLog)->ShouldLog(LogLevel::Debug)) {
    return;
  }

  nsAutoCString logMsg;
  logMsg.AppendPrintf("Building sandbox for utility process:\n");
  logMsg.AppendPrintf("\tJob Level: %d\n", static_cast<int>(us.mJobLevel));
  logMsg.AppendPrintf("\tInitial Token Level: %d\n",
                      static_cast<int>(us.mInitialTokenLevel));
  logMsg.AppendPrintf("\tDelayed Token Level: %d\n",
                      static_cast<int>(us.mDelayedTokenLevel));
  logMsg.AppendPrintf("\tInitial Integrity Level: %d\n",
                      static_cast<int>(us.mInitialIntegrityLevel));
  logMsg.AppendPrintf("\tDelayed Integrity Level: %d\n",
                      static_cast<int>(us.mDelayedIntegrityLevel));
  logMsg.AppendPrintf("\tUse Alternate Window Station: %s\n",
                      us.mUseAlternateWindowStation ? "yes" : "no");
  logMsg.AppendPrintf("\tUse Alternate Desktop: %s\n",
                      us.mUseAlternateDesktop ? "yes" : "no");
  logMsg.AppendPrintf("\tLockdown Default Dacl: %s\n",
                      us.mLockdownDefaultDacl ? "yes" : "no");
  logMsg.AppendPrintf("\tAdd Random Restricting SID: %s\n",
                      us.mAddRestrictingRandomSid ? "yes" : "no");
  logMsg.AppendPrintf("\tUse Win32k Lockdown: %s\n",
                      us.mUseWin32kLockdown ? "yes" : "no");
  logMsg.AppendPrintf("\tUse CIG: %s\n", us.mUseCig ? "yes" : "no");
  logMsg.AppendPrintf("\tInitial mitigations: %016llx\n",
                      static_cast<uint64_t>(us.mInitialMitigations));
  logMsg.AppendPrintf("\tDelayed mitigations: %016llx\n",
                      static_cast<uint64_t>(us.mDelayedMitigations));
  if (us.mPackagePrefix.IsEmpty()) {
    logMsg.AppendPrintf("\tNo Low Privileged Application Container\n");
  } else {
    logMsg.AppendPrintf("\tLow Privileged Application Container Settings:\n");
    logMsg.AppendPrintf("\t\tPackage Name Prefix: %S\n",
                        static_cast<wchar_t*>(us.mPackagePrefix.get()));
    logMsg.AppendPrintf("\t\tWell Known Capabilities:\n");
    for (auto wkCap : us.mWellKnownCapabilites) {
      logMsg.AppendPrintf("\t\t\t%s\n", WellKnownCapabilityNames[wkCap]);
    }
    logMsg.AppendPrintf("\t\tNamed Capabilities:\n");
    for (auto namedCap : us.mNamedCapabilites) {
      logMsg.AppendPrintf("\t\t\t%S\n", namedCap);
    }
  }

  LOG_D("%s", logMsg.get());
}

bool BuildUtilitySandbox(sandbox::TargetPolicy* policy,
                         const UtilitySandboxProps& us) {
  LogUtilitySandboxProps(us);

  auto result = policy->SetJobLevel(us.mJobLevel, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");

  result = policy->SetTokenLevel(us.mInitialTokenLevel, us.mDelayedTokenLevel);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  if (us.mInitialIntegrityLevel != sandbox::INTEGRITY_LEVEL_LAST) {
    result = policy->SetIntegrityLevel(us.mInitialIntegrityLevel);
    SANDBOX_ENSURE_SUCCESS(result,
                           "SetIntegrityLevel should never fail with these "
                           "arguments, what happened?");
  }

  if (us.mDelayedIntegrityLevel != sandbox::INTEGRITY_LEVEL_LAST) {
    result = policy->SetDelayedIntegrityLevel(us.mDelayedIntegrityLevel);
    SANDBOX_ENSURE_SUCCESS(result,
                           "SetIntegrityLevel should never fail with these "
                           "arguments, what happened?");
  }

  if (us.mUseAlternateDesktop) {
    result = policy->SetAlternateDesktop(us.mUseAlternateWindowStation);
    if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
      LOG_W("SetAlternateDesktop failed, result: %i, last error: %lx", result,
            ::GetLastError());
    }
  }

  if (us.mLockdownDefaultDacl) {
    policy->SetLockdownDefaultDacl();
  }
  if (us.mAddRestrictingRandomSid) {
    policy->AddRestrictingRandomSid();
  }

  result = policy->SetProcessMitigations(us.mInitialMitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  result = policy->SetDelayedProcessMitigations(us.mDelayedMitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  // Win32k lockdown might not work on earlier versions
  // Bug 1719212, 1769992
  if (us.mUseWin32kLockdown && IsWin10FallCreatorsUpdateOrLater()) {
    result = AddWin32kLockdownPolicy(policy, false);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to add the win32k lockdown policy");
  }

  if (us.mUseCig) {
    bool alwaysProxyBinDirLoading = mozilla::HasPackageIdentity();
    result = AddCigToPolicy(policy, alwaysProxyBinDirLoading);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to initialize signed policy rules.");
  }

  // Process fails to start in LPAC with ASan build
#if !defined(MOZ_ASAN)
  if (!us.mPackagePrefix.IsEmpty()) {
    MOZ_ASSERT(us.mInitialIntegrityLevel == sandbox::INTEGRITY_LEVEL_LAST,
               "Initial integrity level cannot be specified if using an LPAC.");

    result = AddAndConfigureAppContainerProfile(policy, us.mPackagePrefix,
                                                us.mWellKnownCapabilites,
                                                us.mNamedCapabilites);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to configure AppContainer profile.");
  }
#endif

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}

bool SandboxBroker::SetSecurityLevelForUtilityProcess(
    mozilla::ipc::SandboxingKind aSandbox) {
  if (!mPolicy) {
    return false;
  }

  switch (aSandbox) {
    case mozilla::ipc::SandboxingKind::GENERIC_UTILITY:
      return BuildUtilitySandbox(mPolicy, GenericUtilitySandboxProps());
    case mozilla::ipc::SandboxingKind::UTILITY_AUDIO_DECODING_WMF:
      return BuildUtilitySandbox(mPolicy,
                                 UtilityAudioDecodingWmfSandboxProps());
#ifdef MOZ_WMF_MEDIA_ENGINE
    case mozilla::ipc::SandboxingKind::MF_MEDIA_ENGINE_CDM:
      return BuildUtilitySandbox(mPolicy,
                                 UtilityMfMediaEngineCdmSandboxProps());
#endif
    case mozilla::ipc::SandboxingKind::WINDOWS_UTILS:
      return BuildUtilitySandbox(mPolicy, WindowsUtilitySandboxProps());
    case mozilla::ipc::SandboxingKind::WINDOWS_FILE_DIALOG:
      // This process type is not sandboxed. (See commentary in
      // `ipc::IsUtilitySandboxEnabled()`.)
      MOZ_ASSERT_UNREACHABLE("No sandboxing for this process type");
      return false;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown sandboxing value");
      return false;
  }
}

bool SandboxBroker::SetSecurityLevelForGMPlugin(SandboxLevel aLevel,
                                                bool aIsRemoteLaunch) {
  if (!mPolicy) {
    return false;
  }

  auto result =
      mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");
  auto level = (aLevel == Restricted) ? sandbox::USER_RESTRICTED
                                      : sandbox::USER_LOCKDOWN;
  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS, level);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %lx", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  MOZ_ASSERT(sandbox::SBOX_ALL_OK == result,
             "SetIntegrityLevel should never fail with these arguments, what "
             "happened?");

  result =
      mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP;

  if (StaticPrefs::security_sandbox_gmp_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  if (StaticPrefs::security_sandbox_gmp_win32k_disable()) {
    result = AddWin32kLockdownPolicy(mPolicy, true);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

#ifdef DEBUG
  // The plugin process can't create named events, but we'll
  // make an exception for the events used in logging. Removing
  // this will break EME in debug builds.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                            sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                            L"ChromeIPCLog.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
#endif

  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions caused
  // plugin loading to fail, so they are necessary for proper functioning
  // of at least one EME plugin.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\LanguageConfiguration");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_"
      L"MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions did not
  // cause anything to break during initial testing, but might cause
  // unforeseen issues down the road.
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\MUI\\Settings");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Software\\Policies\\Microsoft\\Control "
      L"Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\PreferredUILanguages");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_"
                            L"MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVer"
                            L"sion\\SideBySide\\PreferExternalManifest");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The following rules were added to allow a GMP to be loaded when any
  // AppLocker DLL rules are specified. If the rules specifically block the DLL
  // then it will not load.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                            L"\\Device\\SrpDevice");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\GP\\");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
  // On certain Windows versions there is a double slash before GP in the path.
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\\\GP\\");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The GMP process needs to be able to share memory with the main process for
  // crash reporting. On arm64 when we are launching remotely via an x86 broker,
  // we need the rule to be HANDLES_DUP_ANY, because we still need to duplicate
  // to the main process not the child's broker.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            aIsRemoteLaunch
                                ? sandbox::TargetPolicy::HANDLES_DUP_ANY
                                : sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}
#undef SANDBOX_ENSURE_SUCCESS

bool SandboxBroker::AllowReadFile(wchar_t const* file) {
  if (!mPolicy) {
    return false;
  }

  auto result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                       sandbox::TargetPolicy::FILES_ALLOW_READONLY, file);
  if (sandbox::SBOX_ALL_OK != result) {
    LOG_E("Failed (ResultCode %d) to add read access to: %S", result, file);
    return false;
  }

  return true;
}

/* static */
bool SandboxBroker::AddTargetPeer(HANDLE aPeerProcess) {
  if (!sBrokerService) {
    return false;
  }

  sandbox::ResultCode result = sBrokerService->AddTargetPeer(aPeerProcess);
  return (sandbox::SBOX_ALL_OK == result);
}

void SandboxBroker::AddHandleToShare(HANDLE aHandle) {
  mPolicy->AddHandleToShare(aHandle);
}

bool SandboxBroker::IsWin32kLockedDown() {
  return mPolicy->GetProcessMitigations() & sandbox::MITIGATION_WIN32K_DISABLE;
}

void SandboxBroker::ApplyLoggingPolicy() {
  MOZ_ASSERT(mPolicy);

  // Add dummy rules, so that we can log in the interception code.
  // We already have a file interception set up for the client side of pipes.
  // Also, passing just "dummy" for file system policy causes win_utils.cc
  // IsReparsePoint() to loop.
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                   sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                   sandbox::TargetPolicy::PROCESS_MIN_EXEC, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                   sandbox::TargetPolicy::REG_ALLOW_READONLY,
                   L"HKEY_CURRENT_USER\\dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                   sandbox::TargetPolicy::EVENTS_ALLOW_READONLY, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                   sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"dummy");
}

SandboxBroker::~SandboxBroker() {
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

}  // namespace mozilla
