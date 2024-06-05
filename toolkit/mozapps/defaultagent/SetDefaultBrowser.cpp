/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <appmodel.h>
#include <shlobj.h>  // for SHChangeNotify and IApplicationAssociationRegistration
#include <timeapi.h>

#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "WindowsUserChoice.h"

#include "EventLog.h"
#include "SetDefaultBrowser.h"

namespace mozilla::default_agent {

/*
 * The implementation for setting extension handlers by writing UserChoice.
 *
 * This is used by both SetDefaultBrowserUserChoice and
 * SetDefaultExtensionHandlersUserChoice.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @param aSid Current user's string SID
 *
 * @param aRegRename True if we should rename registry keys to prevent
 * interference from kernel drivers attempting to lock subkeys.
 *
 * @param aExtraFileExtensions Optional array of extra file association pairs to
 * set as default, like `[ ".pdf", "FirefoxPDF" ]`.
 *
 * @returns NS_OK                  All associations set and checked
 *                                 successfully.
 *          NS_ERROR_WDBA_REJECTED UserChoice was set, but checking the default
 *                                 did not return our ProgID.
 *          NS_ERROR_FAILURE       Failed to set at least one association.
 */
static nsresult SetDefaultExtensionHandlersUserChoiceImpl(
    const wchar_t* aAumi, const wchar_t* const aSid, const bool aRegRename,
    const nsTArray<nsString>& aFileExtensions);

static bool AddMillisecondsToSystemTime(SYSTEMTIME& aSystemTime,
                                        ULONGLONG aIncrementMS) {
  FILETIME fileTime;
  ULARGE_INTEGER fileTimeInt;
  if (!::SystemTimeToFileTime(&aSystemTime, &fileTime)) {
    return false;
  }
  fileTimeInt.LowPart = fileTime.dwLowDateTime;
  fileTimeInt.HighPart = fileTime.dwHighDateTime;

  // FILETIME is in units of 100ns.
  fileTimeInt.QuadPart += aIncrementMS * 1000 * 10;

  fileTime.dwLowDateTime = fileTimeInt.LowPart;
  fileTime.dwHighDateTime = fileTimeInt.HighPart;
  SYSTEMTIME tmpSystemTime;
  if (!::FileTimeToSystemTime(&fileTime, &tmpSystemTime)) {
    return false;
  }

  aSystemTime = tmpSystemTime;
  return true;
}

/*
 * Takes two times: the start time and the current time, and returns the number
 * of seconds left before the current time hits the next minute from the start
 * time. Used to check if we are within the same minute as the start time and
 * how much time we have left to perform an operation in the same minute.
 *
 * Used with user choice hashes, which have to be written to the registry
 * in the same minute as they are generated.
 *
 * Example 1:
 * operationStartTime - 10m 20s 800ms
 * currentTime - 10m 22s 0ms
 * The next minute is 11, so the return value is 11m - 10m 22s 0ms, converted to
 * milliseconds.
 *
 * Example 2:
 * operationStartTime - 10m 59s 800ms
 * currentTime - 11m 0s 0ms
 * The next minute is 11, but the minute the operation started on was 10, so the
 * time to the next minute is 0 (because the current time is already at the next
 * minute).
 *
 * @param operationStartTime
 * @param currentTime
 *
 * @returns     the number of milliseconds left from the current time to the
 * next minute from operationStartTime, or zero if the currentTime is already at
 * the next minute or greater
 */
static WORD GetMillisecondsToNextMinute(SYSTEMTIME operationStartTime,
                                        SYSTEMTIME currentTime) {
  SYSTEMTIME operationStartTimeMinute = operationStartTime;
  SYSTEMTIME currentTimeMinute = currentTime;

  // Zero out the seconds and milliseconds so that we can confirm they are the
  // same minutes
  operationStartTimeMinute.wSecond = 0;
  operationStartTimeMinute.wMilliseconds = 0;

  currentTimeMinute.wSecond = 0;
  currentTimeMinute.wMilliseconds = 0;

  // Convert to a 64 bit value so we can compare them directly
  FILETIME fileTime1;
  FILETIME fileTime2;
  if (!::SystemTimeToFileTime(&operationStartTimeMinute, &fileTime1) ||
      !::SystemTimeToFileTime(&currentTimeMinute, &fileTime2)) {
    // Error: report that there is 0 milliseconds till the next minute
    return 0;
  }

  // The minutes for both times have to be the same, so confirm that they are,
  // and if they aren't, return 0 milliseconds to indicate that we're already
  // not on the minute that operationStartTime was on
  if ((fileTime1.dwLowDateTime != fileTime2.dwLowDateTime) ||
      (fileTime1.dwHighDateTime != fileTime2.dwHighDateTime)) {
    return 0;
  }

  // The minutes are the same; determine the number of milliseconds left until
  // the next minute
  const WORD secondsToMilliseconds = 1000;
  const WORD minutesToSeconds = 60;

  // 1 minute converted to milliseconds - (the current second converted to
  // milliseconds + the current milliseconds)
  return (1 * minutesToSeconds * secondsToMilliseconds) -
         ((currentTime.wSecond * secondsToMilliseconds) +
          currentTime.wMilliseconds);
}

// Compare two SYSTEMTIMEs as FILETIME after clearing everything
// below minutes.
static bool CheckEqualMinutes(SYSTEMTIME aSystemTime1,
                              SYSTEMTIME aSystemTime2) {
  aSystemTime1.wSecond = 0;
  aSystemTime1.wMilliseconds = 0;

  aSystemTime2.wSecond = 0;
  aSystemTime2.wMilliseconds = 0;

  FILETIME fileTime1;
  FILETIME fileTime2;
  if (!::SystemTimeToFileTime(&aSystemTime1, &fileTime1) ||
      !::SystemTimeToFileTime(&aSystemTime2, &fileTime2)) {
    return false;
  }

  return (fileTime1.dwLowDateTime == fileTime2.dwLowDateTime) &&
         (fileTime1.dwHighDateTime == fileTime2.dwHighDateTime);
}

static bool SetUserChoiceRegistry(const wchar_t* aExt, const wchar_t* aProgID,
                                  const bool aRegRename,
                                  mozilla::UniquePtr<wchar_t[]> aHash) {
  auto assocKeyPath = GetAssociationKeyPath(aExt);
  if (!assocKeyPath) {
    return false;
  }

  LSTATUS ls;
  HKEY rawAssocKey;
  ls = ::RegOpenKeyExW(HKEY_CURRENT_USER, assocKeyPath.get(), 0,
                       KEY_READ | KEY_WRITE, &rawAssocKey);
  if (ls != ERROR_SUCCESS) {
    LOG_ERROR(HRESULT_FROM_WIN32(ls));
    return false;
  }
  nsAutoRegKey assocKey(rawAssocKey);

  if (aRegRename) {
    // Registry keys in the HKCU should be writable by applications run by the
    // hive owning user and should not be lockable -
    // https://web.archive.org/web/20230308044345/https://devblogs.microsoft.com/oldnewthing/20090326-00/?p=18703.
    // Unfortunately some kernel drivers lock a set of protocol and file
    // association keys. Renaming a non-locked ancestor is sufficient to fix
    // this.
    nsAutoString tempName =
        NS_ConvertASCIItoUTF16(nsID::GenerateUUID().ToString().get());
    ls = ::RegRenameKey(assocKey.get(), nullptr, tempName.get());
    if (ls != ERROR_SUCCESS) {
      LOG_ERROR(HRESULT_FROM_WIN32(ls));
      return false;
    }
  }

  auto subkeysUpdated = [&] {
    // Windows file association keys are read-only (Deny Set Value) for the
    // User, meaning they can not be modified but can be deleted and recreated.
    // We don't set any similar special permissions. Note: this only applies to
    // file associations, not URL protocols.
    if (aExt && aExt[0] == '.') {
      ls = ::RegDeleteKeyW(assocKey.get(), L"UserChoice");
      if (ls != ERROR_SUCCESS) {
        LOG_ERROR(HRESULT_FROM_WIN32(ls));
        return false;
      }
    }

    HKEY rawUserChoiceKey;
    ls = ::RegCreateKeyExW(assocKey.get(), L"UserChoice", 0, nullptr,
                           0 /* options */, KEY_READ | KEY_WRITE,
                           0 /* security attributes */, &rawUserChoiceKey,
                           nullptr);
    if (ls != ERROR_SUCCESS) {
      LOG_ERROR(HRESULT_FROM_WIN32(ls));
      return false;
    }
    nsAutoRegKey userChoiceKey(rawUserChoiceKey);

    DWORD progIdByteCount = (::lstrlenW(aProgID) + 1) * sizeof(wchar_t);
    ls = ::RegSetValueExW(userChoiceKey.get(), L"ProgID", 0, REG_SZ,
                          reinterpret_cast<const unsigned char*>(aProgID),
                          progIdByteCount);
    if (ls != ERROR_SUCCESS) {
      LOG_ERROR(HRESULT_FROM_WIN32(ls));
      return false;
    }

    DWORD hashByteCount = (::lstrlenW(aHash.get()) + 1) * sizeof(wchar_t);
    ls = ::RegSetValueExW(userChoiceKey.get(), L"Hash", 0, REG_SZ,
                          reinterpret_cast<const unsigned char*>(aHash.get()),
                          hashByteCount);
    if (ls != ERROR_SUCCESS) {
      LOG_ERROR(HRESULT_FROM_WIN32(ls));
      return false;
    }

    return true;
  }();

  if (aRegRename) {
    // Rename back regardless of whether we successfully modified the subkeys to
    // minimally attempt to return the registry the way we found it. If this
    // fails the afformetioned kernel drivers have likely changed and there's
    // little we can do to anticipate what proper recovery should look like.
    ls = ::RegRenameKey(assocKey.get(), nullptr, aExt);
    if (ls != ERROR_SUCCESS) {
      LOG_ERROR(HRESULT_FROM_WIN32(ls));
      return false;
    }
  }

  return subkeysUpdated;
}

struct LaunchExeErr {};
using LaunchExeResult =
    mozilla::Result<std::tuple<DWORD, mozilla::UniquePtr<wchar_t[]>>,
                    LaunchExeErr>;

static LaunchExeResult LaunchExecutable(const wchar_t* exePath, int aArgsLength,
                                        const wchar_t* const* aArgs) {
  const wchar_t* args[] = {exePath};
  mozilla::UniquePtr<wchar_t[]> cmdLine(mozilla::MakeCommandLine(
      mozilla::ArrayLength(args), const_cast<wchar_t**>(args), aArgsLength,
      const_cast<wchar_t**>(aArgs)));

  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  if (!::CreateProcessW(exePath, cmdLine.get(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return Err(LaunchExeErr());
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  DWORD exitCode;
  if (::WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0 &&
      ::GetExitCodeProcess(process.get(), &exitCode)) {
    return std::make_tuple(exitCode, std::move(cmdLine));
  }

  return Err(LaunchExeErr());
}

static bool LaunchPowershell(const wchar_t* command,
                             const wchar_t* powershellPath) {
  const wchar_t* args[] = {
      L"-NoProfile",  // ensure nothing is monkeying with powershell
      L"-c",
      command,
  };

  return LaunchExecutable(powershellPath, mozilla::ArrayLength(args), args)
      .map([](auto result) {
        auto& [exitCode, regCmdLine] = result;
        bool success = (exitCode == 0);
        if (!success) {
          LOG_ERROR_MESSAGE(L"%s returned failure exitCode %d",
                            regCmdLine.get(), exitCode);
        }
        return success;
      })
      .unwrapOr(false);
}

static bool FindPowershell(mozilla::UniquePtr<wchar_t[]>& powershellPath) {
  auto exePath = mozilla::MakeUnique<wchar_t[]>(MAX_PATH + 1);
  if (!ConstructSystem32Path(L"WindowsPowershell\\v1.0\\powershell.exe",
                             exePath.get(), MAX_PATH + 1)) {
    LOG_ERROR_MESSAGE(L"Failed to construct path to powershell.exe");
    return false;
  }

  powershellPath.swap(exePath);
  return true;
}

/*
 * Set an association with a UserChoice key
 *
 * Removes the old key, creates a new one with ProgID and Hash set to
 * enable a new asociation.
 *
 * @param aExt      File type or protocol to associate
 * @param aSid      Current user's string SID
 * @param aProgID   ProgID to use for the asociation
 * @param inMsix    Are we running from in an msix package?
 * @param aRegRename True if we should rename registry keys to prevent
 * interference from kernel drivers attempting to lock subkeys.
 *
 * @return true if successful, false on error.
 */
static bool SetUserChoice(const wchar_t* aExt, const wchar_t* aSid,
                          const wchar_t* aProgID, bool inMsix,
                          const bool aRegRename) {
  if (inMsix) {
    LOG_ERROR_MESSAGE(
        L"SetUserChoice should not be called on MSIX builds. Call "
        L"SetDefaultExtensionHandlersUserChoiceImplMsix instead.");
    return false;
  }

  SYSTEMTIME hashTimestamp;
  ::GetSystemTime(&hashTimestamp);
  auto hash = GenerateUserChoiceHash(aExt, aSid, aProgID, hashTimestamp);
  if (!hash) {
    return false;
  }

  // The hash changes at the end of each minute, so check that the hash should
  // be the same by the time we're done writing.
  const ULONGLONG kWriteTimingThresholdMilliseconds = 1000;
  // Generating the hash could have taken some time, so start from now.
  SYSTEMTIME writeEndTimestamp;
  ::GetSystemTime(&writeEndTimestamp);
  if (!AddMillisecondsToSystemTime(writeEndTimestamp,
                                   kWriteTimingThresholdMilliseconds)) {
    return false;
  }
  if (!CheckEqualMinutes(hashTimestamp, writeEndTimestamp)) {
    LOG_ERROR_MESSAGE(
        L"Hash is too close to expiration, sleeping until next hash.");
    ::Sleep(kWriteTimingThresholdMilliseconds * 2);

    // For consistency, use the current time.
    ::GetSystemTime(&hashTimestamp);
    hash = GenerateUserChoiceHash(aExt, aSid, aProgID, hashTimestamp);
    if (!hash) {
      return false;
    }
  }

  // We're outside of an MSIX package and can use the Win32 Registry API.
  return SetUserChoiceRegistry(aExt, aProgID, aRegRename, std::move(hash));
}

static bool VerifyUserDefault(const wchar_t* aExt, const wchar_t* aProgID) {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = ::CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return false;
  }

  wchar_t* rawRegisteredApp;
  bool isProtocol = aExt[0] != L'.';
  // Note: Checks AL_USER instead of AL_EFFECTIVE.
  hr = pAAR->QueryCurrentDefault(aExt,
                                 isProtocol ? AT_URLPROTOCOL : AT_FILEEXTENSION,
                                 AL_USER, &rawRegisteredApp);
  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION)) {
      LOG_ERROR_MESSAGE(L"UserChoice ProgID %s for %s was rejected", aProgID,
                        aExt);
    } else {
      LOG_ERROR(hr);
    }
    return false;
  }
  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> registeredApp(
      rawRegisteredApp);

  if (::CompareStringOrdinal(registeredApp.get(), -1, aProgID, -1, FALSE) !=
      CSTR_EQUAL) {
    LOG_ERROR_MESSAGE(
        L"Default was %s after writing ProgID %s to UserChoice for %s",
        registeredApp.get(), aProgID, aExt);
    return false;
  }

  return true;
}

nsresult SetDefaultBrowserUserChoice(
    const wchar_t* aAumi, const bool aRegRename,
    const nsTArray<nsString>& aExtraFileExtensions) {
  // Verify that the implementation of UserChoice hashing has not changed by
  // computing the current default hash and comparing with the existing value.
  if (!CheckBrowserUserChoiceHashes()) {
    LOG_ERROR_MESSAGE(L"UserChoice Hash mismatch");
    return NS_ERROR_WDBA_HASH_CHECK;
  }

  if (!mozilla::IsWin10CreatorsUpdateOrLater()) {
    LOG_ERROR_MESSAGE(L"UserChoice hash matched, but Windows build is too old");
    return NS_ERROR_WDBA_BUILD;
  }

  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsString> browserDefaults = {
      u"https"_ns, u"FirefoxURL"_ns,  u"http"_ns, u"FirefoxURL"_ns,
      u".html"_ns, u"FirefoxHTML"_ns, u".htm"_ns, u"FirefoxHTML"_ns};

  browserDefaults.AppendElements(aExtraFileExtensions);

  nsresult rv = SetDefaultExtensionHandlersUserChoiceImpl(
      aAumi, sid.get(), aRegRename, browserDefaults);
  if (!NS_SUCCEEDED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  return rv;
}

nsresult SetDefaultExtensionHandlersUserChoice(
    const wchar_t* aAumi, const bool aRegRename,
    const nsTArray<nsString>& aFileExtensions) {
  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = SetDefaultExtensionHandlersUserChoiceImpl(
      aAumi, sid.get(), aRegRename, aFileExtensions);
  if (!NS_SUCCEEDED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  return rv;
}

/*
 * Takes the list of file extension pairs and fills a list of program ids for
 * each pair.
 *
 * @param aFileExtensions array of file association pairs to
 * set as default, like `[ ".pdf", "FirefoxPDF" ]`.
 */
static nsresult GenerateProgramIDs(const nsTArray<nsString>& aFileExtensions,
                                   nsTArray<nsString>& progIDs) {
  for (size_t i = 0; i + 1 < aFileExtensions.Length(); i += 2) {
    const wchar_t* fileExtension = aFileExtensions[i].get();

    // Formatting the ProgID here prevents using this helper to target arbitrary
    // ProgIDs.
    mozilla::UniquePtr<wchar_t[]> progID;
    nsresult rv = GetMsixProgId(fileExtension, progID);
    if (NS_FAILED(rv)) {
      LOG_ERROR_MESSAGE(L"Failed to retrieve MSIX progID for %s",
                        fileExtension);
      return rv;
    }

    progIDs.AppendElement(nsString(progID.get()));
  }

  return NS_OK;
}

/*
 * Takes the list of file extension pairs and a matching list of program ids for
 * each of those pairs and verifies that the system is successfully to match
 * each.
 *
 * @param aFileExtensions array of file association pairs to set as default,
 * like `[ ".pdf", "FirefoxPDF" ]`.
 * @param aProgIDs array of program ids. The order of this array matches the
 * file extensions parameter.
 */
static nsresult VerifyUserDefaults(const nsTArray<nsString>& aFileExtensions,
                                   const nsTArray<nsString>& aProgIDs) {
  for (size_t i = 0; i + 1 < aFileExtensions.Length(); i += 2) {
    const wchar_t* fileExtension = aFileExtensions[i].get();

    if (!VerifyUserDefault(fileExtension, aProgIDs[i / 2].get())) {
      return NS_ERROR_WDBA_REJECTED;
    }
  }

  return NS_OK;
}

/*
 * Queries the system for the minimum resolution (or tick time) in milliseconds
 * that can be used with ::Sleep. If a Sleep is not triggered with a time
 * divisible by the tick time, control can return to the thread before the time
 * specified to Sleep.
 *
 * @param defaultValue  what to return if the system query fails.
 */
static UINT GetSystemSleepIntervalInMilliseconds(UINT defaultValue) {
  TIMECAPS timeCapabilities;
  bool timeCapsFetchSuccessful =
      (MMSYSERR_NOERROR ==
       timeGetDevCaps(&timeCapabilities, sizeof(timeCapabilities)));
  if (!timeCapsFetchSuccessful) {
    return defaultValue;
  }

  return timeCapabilities.wPeriodMin > 0 ? timeCapabilities.wPeriodMin
                                         : defaultValue;
}

/*
 * MSIX implementation for SetDefaultExtensionHandlersUserChoice.
 *
 * Due to the fact that MSIX builds run in a virtual, walled off environment,
 * calling into the Win32 registry APIs doesn't work to set registry keys.
 * MSIX builds access a virtual registry.
 *
 * Sends a "script" via command line to Powershell to modify the registry
 * in an executable that is outside of the walled garden MSIX FX package
 * environment, which is the only way to do that so far. Only works
 * on slightly older versions of Windows 11 (older than 23H2).
 *
 * This was originally done using calls to Reg.exe, but permissions can't
 * be changed that way, requiring using Powershell to call into .Net functions
 * directly. Launching Powershell is slow (on the order of a second on some
 * systems) so this method jams the calls to do everything into one launch of
 * Powershell, to make it as quick as possible.
 *
 */
static nsresult SetDefaultExtensionHandlersUserChoiceImplMsix(
    const wchar_t* aAumi, const wchar_t* const aSid, const bool aRegRename,
    const nsTArray<nsString>& aFileExtensions) {
  mozilla::UniquePtr<wchar_t[]> exePath;
  if (!FindPowershell(exePath)) {
    LOG_ERROR_MESSAGE(L"Could not locate Powershell");
    return NS_ERROR_FAILURE;
  }

  // The following is the start of the script that will be sent to Powershell.
  // In includes a function; calls to the function get appended per file
  // extension, done below.
  auto startScript =
      uR"(
# Force exceptions to stop execution
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
 using System;
 using System.Runtime.InteropServices;

 public static class WinReg
 {
   [DllImport("Advapi32.dll")]
   public static extern Int32 RegRenameKey(
     IntPtr hKey,
     [MarshalAs(UnmanagedType.LPWStr)] string lpSubKeyName,
     [MarshalAs(UnmanagedType.LPWStr)] string lpNewKeyName
   );
 }
"@

function Set-DefaultHandlerRegistry($Association, $Path, $ProgID, $Hash, $RegRename) {
  $RootKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($Path)

  if ($RegRename) {
    # Rename the registry key.
    $TempName = New-Guid
    $Handle = $RootKey.Handle.DangerousGetHandle()
    $result = [WinReg]::RegRenameKey($Handle, $null, $TempName.ToString())
    if ($result -ne 0) {
      throw "Error renaming key to temporary value."
    }
  }

  # DeleteSubKey throws if we don't have sufficient permissions to delete key,
  # signaling failure to launching process.
  #
  # Note: DeleteSubKeyTree fails when DENY permissions are set on key, whereas
  # DeleteSubKey succeeds.
  $RootKey.DeleteSubKey("UserChoice", $false)
  $UserChoice = $RootKey.CreateSubKey("UserChoice")

  $StringType = [Microsoft.Win32.RegistryValueKind]::String
  $UserChoice.SetValue('ProgID', $ProgID, $StringType)
  $UserChoice.SetValue('Hash', $Hash, $StringType)

  if ($RegRename) {
    $result = [WinReg]::RegRenameKey($Handle, $null, $Association)
    if ($result -ne 0) {
      throw "Error renaming key to association value."
    }
  }
}

)"_ns;  // Newlines in the above powershell script at the end are important!!!

  // NOTE!!!! CreateProcess / calling things on the command line has a character
  // count limit. For CMD.exe, it's 8K. For CreateProcessW, it's technically
  // 32K. I can't find documentation about Powershell, but think 8K is safe to
  // assume as a good maximum. The above script is about 1000 characters, and we
  // will append another 100 characters at most per extension, so we'd need to
  // be setting about 70 handlers at once to worry about the theoretical limit.
  // Which we won't do. So the length shouldn't be a problem.
  if (aFileExtensions.Length() >= 70) {
    LOG_ERROR_MESSAGE(
        L"SetDefaultExtensionHandlersUserChoiceImplMsix can't cope with 70 or "
        L"more file extensions at once. Please break it up into multiple calls "
        L"with fewer extensions per call.");
    return NS_ERROR_FAILURE;
  }

  // NOTE!!!!
  // User choice hashes have to be generated and written to the registry in the
  // same minute. So we do everything we can upfront before we get to the hash
  // generation, to ensure that hash generation and the call to Powershell to
  // write to the registry has as much time as possible to run.

  // Program ID fetch / generation might be slow, so do that ahead of time.
  nsTArray<nsString> progIDs;
  nsresult rv = GenerateProgramIDs(aFileExtensions, progIDs);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString scriptBuffer;

  // Everyting in the loop below should succeed or fail reasonably fast (within
  // 20 milliseconds or something well under a second) besides the Powershell
  // call. The Powershell call itself will either fail or succeed and break out
  // of the loop, so repeating 10 times should be fine and is mostly to allow
  // for getting the timing right with the user choice hash generation happening
  // in the same minute - meaning this should likely only happen twice through
  // the loop at most.
  for (int i = 0; i < 10; i++) {
    // Pre-allocate the memory for the scriptBuffer upfront so that we don't
    // have to keep allocating every time Append is called.
    const int scriptBufferCapacity = 16 * 1024;
    scriptBuffer = startScript;
    scriptBuffer.SetCapacity(scriptBufferCapacity);

    SYSTEMTIME hashTimestamp;
    ::GetSystemTime(&hashTimestamp);

    // Time critical stuff starts here:

    for (size_t i = 0; i + 1 < aFileExtensions.Length(); i += 2) {
      const wchar_t* fileExtension = aFileExtensions[i].get();

      auto association = nsDependentString(fileExtension);

      nsAutoString keyPath;
      AppendAssociationKeyPath(fileExtension, keyPath);

      auto hashWchar = GenerateUserChoiceHash(
          fileExtension, aSid, progIDs[i / 2].get(), hashTimestamp);
      if (!hashWchar) {
        return NS_ERROR_FAILURE;
      }
      auto hash = nsDependentString(hashWchar.get());

      auto regRename = aRegRename ? u"$TRUE"_ns : u"$FALSE"_ns;

      // Append a line to the script buffer in the form:
      // Set-DefaultHandlerRegistry $Association $RegistryKeyPath $ProgID
      // $UserChoiceHash $RegRename
      scriptBuffer += u"Set-DefaultHandlerRegistry "_ns + association +
                      u" "_ns + keyPath + u" "_ns + progIDs[i / 2] + u" "_ns +
                      hash + u" "_ns + regRename + u"\n"_ns;
    }

    // The hash changes at the end of each minute, so check that the hash should
    // be the same by the time we're done writing.
    const ULONGLONG kWriteTimingThresholdMilliseconds = 2000;

    // Generating the hash could have taken some time, so figure out what time
    // we are at right now.
    SYSTEMTIME writeEndTimestamp;
    ::GetSystemTime(&writeEndTimestamp);

    // Check if we have enough time to launch Powershell or if we should sleep
    // to the next minute and try again
    auto millisecondsLeftUntilNextMinute =
        GetMillisecondsToNextMinute(hashTimestamp, writeEndTimestamp);
    if (millisecondsLeftUntilNextMinute >= kWriteTimingThresholdMilliseconds) {
      break;
    }

    LOG_ERROR_MESSAGE(
        L"Hash is too close to next minute, sleeping until next minute to "
        L"ensure that hash generation matches write to registry.");

    UINT sleepUntilNextMinuteBufferMilliseconds =
        GetSystemSleepIntervalInMilliseconds(
            50);  // Default to 50ms if we can't figure out the right interval
                  // to sleep for
    ::Sleep(millisecondsLeftUntilNextMinute +
            (sleepUntilNextMinuteBufferMilliseconds * 2));

    // Try again, if we have any attempts left
  }

  // Call Powershell to set the registry keys now - this is the really time
  // consuming thing 250ms to launch on a VM in a reasonably fast case, possibly
  // a lot slower on other systems
  bool powershellSuccessful =
      LaunchPowershell(scriptBuffer.get(), exePath.get());
  if (!powershellSuccessful) {
    // If powershell failed, it likely means that something got mucked with
    // the registry and that Windows is popping up notifications to the user,
    // so don't try again right now, so as to not overwhelm the user and annoy
    // them.
    return NS_ERROR_FAILURE;
  }

  // Validate now
  return VerifyUserDefaults(aFileExtensions, progIDs);
}

nsresult SetDefaultExtensionHandlersUserChoiceImpl(
    const wchar_t* aAumi, const wchar_t* const aSid, const bool aRegRename,
    const nsTArray<nsString>& aFileExtensions) {
  UINT32 pfnLen = 0;
  bool inMsix =
      GetCurrentPackageFullName(&pfnLen, nullptr) != APPMODEL_ERROR_NO_PACKAGE;

  if (inMsix) {
    return SetDefaultExtensionHandlersUserChoiceImplMsix(
        aAumi, aSid, aRegRename, aFileExtensions);
  }

  for (size_t i = 0; i + 1 < aFileExtensions.Length(); i += 2) {
    const wchar_t* extraFileExtension = aFileExtensions[i].get();
    const wchar_t* extraProgIDRoot = aFileExtensions[i + 1].get();
    // Formatting the ProgID here prevents using this helper to target arbitrary
    // ProgIDs.
    mozilla::UniquePtr<wchar_t[]> extraProgID;
    if (inMsix) {
      nsresult rv = GetMsixProgId(extraFileExtension, extraProgID);
      if (NS_FAILED(rv)) {
        LOG_ERROR_MESSAGE(L"Failed to retrieve MSIX progID for %s",
                          extraFileExtension);
        return rv;
      }
    } else {
      extraProgID = FormatProgID(extraProgIDRoot, aAumi);
      if (!CheckProgIDExists(extraProgID.get())) {
        LOG_ERROR_MESSAGE(L"ProgID %s not found", extraProgID.get());
        return NS_ERROR_WDBA_NO_PROGID;
      }
    }

    if (!SetUserChoice(extraFileExtension, aSid, extraProgID.get(), inMsix,
                       aRegRename)) {
      return NS_ERROR_FAILURE;
    }

    if (!VerifyUserDefault(extraFileExtension, extraProgID.get())) {
      return NS_ERROR_WDBA_REJECTED;
    }
  }

  return NS_OK;
}

}  // namespace mozilla::default_agent
