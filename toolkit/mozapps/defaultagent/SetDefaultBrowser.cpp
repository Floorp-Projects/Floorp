/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <appmodel.h>
#include <shlobj.h>  // for SHChangeNotify and IApplicationAssociationRegistration

#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/RefPtr.h"
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
    const wchar_t* aAumi, const wchar_t* const aSid,
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

  // When Windows creates this key, it is read-only (Deny Set Value), so we need
  // to delete it first.
  // We don't set any similar special permissions.
  ls = ::RegDeleteKeyW(assocKey.get(), L"UserChoice");
  if (ls != ERROR_SUCCESS) {
    LOG_ERROR(HRESULT_FROM_WIN32(ls));
    return false;
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
}

static bool LaunchReg(int aArgsLength, const wchar_t* const* aArgs) {
  mozilla::UniquePtr<wchar_t[]> regPath =
      mozilla::MakeUnique<wchar_t[]>(MAX_PATH + 1);
  if (!ConstructSystem32Path(L"reg.exe", regPath.get(), MAX_PATH + 1)) {
    LOG_ERROR_MESSAGE(L"Failed to construct path to reg.exe");
    return false;
  }

  const wchar_t* regArgs[] = {regPath.get()};
  mozilla::UniquePtr<wchar_t[]> regCmdLine(mozilla::MakeCommandLine(
      mozilla::ArrayLength(regArgs), const_cast<wchar_t**>(regArgs),
      aArgsLength, const_cast<wchar_t**>(aArgs)));

  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  if (!::CreateProcessW(regPath.get(), regCmdLine.get(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return false;
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  DWORD exitCode;
  if (::WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0 &&
      ::GetExitCodeProcess(process.get(), &exitCode)) {
    // N.b.: `reg.exe` returns 0 (unchanged) or 2 (changed) on success.
    bool success = (exitCode == 0 || exitCode == 2);
    if (!success) {
      LOG_ERROR_MESSAGE(L"%s returned failure exitCode %d", regCmdLine.get(),
                        exitCode);
    }
    return success;
  }

  return false;
}

static bool SetUserChoiceCommand(const wchar_t* aExt, const wchar_t* aProgID,
                                 const wchar_t* aHash) {
  auto assocKeyPath = GetAssociationKeyPath(aExt);
  if (!assocKeyPath) {
    return false;
  }

  const wchar_t* formatString = L"HKCU\\%s\\UserChoice";
  int bufferSize = _scwprintf(formatString, assocKeyPath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> userChoiceKeyPath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(userChoiceKeyPath.get(), bufferSize, _TRUNCATE, formatString,
               assocKeyPath.get());

  const wchar_t* deleteArgs[] = {
      L"DELETE",
      userChoiceKeyPath.get(),
      L"/F",
  };
  if (!LaunchReg(mozilla::ArrayLength(deleteArgs), deleteArgs)) {
    LOG_ERROR_MESSAGE(L"Failed to reg.exe DELETE; ignoring and continuing.");
  }

  // Like REG ADD [ROOT\]RegKey /V ValueName [/T DataType] [/S Separator] [/D
  // Data] [/F] [/reg:32] [/reg:64]

  const wchar_t* progIDArgs[] = {
      L"ADD",    userChoiceKeyPath.get(),
      L"/F",     L"/V",
      L"ProgID", L"/T",
      L"REG_SZ", L"/D",
      aProgID,
  };

  if (!LaunchReg(mozilla::ArrayLength(progIDArgs), progIDArgs)) {
    // LaunchReg will have logged an error message already.
    return false;
  }

  const wchar_t* hashArgs[] = {
      L"ADD",    userChoiceKeyPath.get(),
      L"/F",     L"/V",
      L"Hash",   L"/T",
      L"REG_SZ", L"/D",
      aHash,
  };
  if (!LaunchReg(mozilla::ArrayLength(hashArgs), hashArgs)) {
    // LaunchReg will have logged an error message already.
    return false;
  }

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
 *
 * @return true if successful, false on error.
 */
static bool SetUserChoice(const wchar_t* aExt, const wchar_t* aSid,
                          const wchar_t* aProgID) {
  // This might be slow to query, so do it before generating timestamps and
  // hashes.
  UINT32 pfnLen = 0;
  bool inMsix =
      GetCurrentPackageFullName(&pfnLen, nullptr) != APPMODEL_ERROR_NO_PACKAGE;

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

  if (inMsix) {
    // We're in an MSIX package, thus need to use reg.exe.
    return SetUserChoiceCommand(aExt, aProgID, hash.get());
  } else {
    // We're outside of an MSIX package and can use the Win32 Registry API.
    return SetUserChoiceRegistry(aExt, aProgID, std::move(hash));
  }
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
    const wchar_t* aAumi, const nsTArray<nsString>& aExtraFileExtensions) {
  // Verify that the implementation of UserChoice hashing has not changed by
  // computing the current default hash and comparing with the existing value.
  if (!CheckBrowserUserChoiceHashes()) {
    LOG_ERROR_MESSAGE(L"UserChoice Hash mismatch");
    return NS_ERROR_WDBA_HASH_CHECK;
  }

  nsTArray<nsString> browserDefaults = {
      u"https"_ns, u"FirefoxURL"_ns,  u"http"_ns, u"FirefoxURL"_ns,
      u".html"_ns, u"FirefoxHTML"_ns, u".htm"_ns, u"FirefoxHTML"_ns};

  browserDefaults.AppendElements(aExtraFileExtensions);

  if (!mozilla::IsWin10CreatorsUpdateOrLater()) {
    LOG_ERROR_MESSAGE(L"UserChoice hash matched, but Windows build is too old");
    return NS_ERROR_WDBA_BUILD;
  }

  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = SetDefaultExtensionHandlersUserChoiceImpl(aAumi, sid.get(),
                                                          browserDefaults);
  if (!NS_SUCCEEDED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  return rv;
}

nsresult SetDefaultExtensionHandlersUserChoice(
    const wchar_t* aAumi, const nsTArray<nsString>& aFileExtensions) {
  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = SetDefaultExtensionHandlersUserChoiceImpl(aAumi, sid.get(),
                                                          aFileExtensions);
  if (!NS_SUCCEEDED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  return rv;
}

nsresult SetDefaultExtensionHandlersUserChoiceImpl(
    const wchar_t* aAumi, const wchar_t* const aSid,
    const nsTArray<nsString>& aFileExtensions) {
  UINT32 pfnLen = 0;
  bool inMsix =
      GetCurrentPackageFullName(&pfnLen, nullptr) != APPMODEL_ERROR_NO_PACKAGE;

  for (size_t i = 0; i + 1 < aFileExtensions.Length(); i += 2) {
    const wchar_t* extraFileExtension =
        PromiseFlatString(aFileExtensions[i]).get();
    const wchar_t* extraProgIDRoot =
        PromiseFlatString(aFileExtensions[i + 1]).get();
    // Formatting the ProgID here prevents using this helper to target arbitrary
    // ProgIDs.
    UniquePtr<wchar_t[]> extraProgID;
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

    if (!SetUserChoice(extraFileExtension, aSid, extraProgID.get())) {
      return NS_ERROR_FAILURE;
    }

    if (!VerifyUserDefault(extraFileExtension, extraProgID.get())) {
      return NS_ERROR_WDBA_REJECTED;
    }
  }

  return NS_OK;
}

}  // namespace mozilla::default_agent
