/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shlobj.h>  // for SHChangeNotify and IApplicationAssociationRegistration

#include "mozilla/ArrayUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "WindowsUserChoice.h"

#include "EventLog.h"
#include "SetDefaultBrowser.h"

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
 * @param aFileExtensions Optional null-terminated list of file association
 * pairs to set as default, like `{ L".pdf", "FirefoxPDF", nullptr }`.
 *
 * @returns S_OK           All associations set and checked successfully.
 *          MOZ_E_REJECTED UserChoice was set, but checking the default did not
 *                         return our ProgID.
 *          E_FAIL         Failed to set at least one association.
 */
static HRESULT SetDefaultExtensionHandlersUserChoiceImpl(
    const wchar_t* aAumi, const wchar_t* const aSid,
    const wchar_t* const* aFileExtensions);

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
  SYSTEMTIME hashTimestamp;
  ::GetSystemTime(&hashTimestamp);
  auto hash = GenerateUserChoiceHash(aExt, aSid, aProgID, hashTimestamp);
  if (!hash) {
    return false;
  }

  // The hash changes at the end of each minute, so check that the hash should
  // be the same by the time we're done writing.
  const ULONGLONG kWriteTimingThresholdMilliseconds = 100;
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

  DWORD hashByteCount = (::lstrlenW(hash.get()) + 1) * sizeof(wchar_t);
  ls = ::RegSetValueExW(userChoiceKey.get(), L"Hash", 0, REG_SZ,
                        reinterpret_cast<const unsigned char*>(hash.get()),
                        hashByteCount);
  if (ls != ERROR_SUCCESS) {
    LOG_ERROR(HRESULT_FROM_WIN32(ls));
    return false;
  }

  return true;
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

HRESULT SetDefaultBrowserUserChoice(
    const wchar_t* aAumi, const wchar_t* const* aExtraFileExtensions) {
  auto urlProgID = FormatProgID(L"FirefoxURL", aAumi);
  if (!CheckProgIDExists(urlProgID.get())) {
    LOG_ERROR_MESSAGE(L"ProgID %s not found", urlProgID.get());
    return MOZ_E_NO_PROGID;
  }

  auto htmlProgID = FormatProgID(L"FirefoxHTML", aAumi);
  if (!CheckProgIDExists(htmlProgID.get())) {
    LOG_ERROR_MESSAGE(L"ProgID %s not found", htmlProgID.get());
    return MOZ_E_NO_PROGID;
  }

  auto pdfProgID = FormatProgID(L"FirefoxPDF", aAumi);
  if (!CheckProgIDExists(pdfProgID.get())) {
    LOG_ERROR_MESSAGE(L"ProgID %s not found", pdfProgID.get());
    return MOZ_E_NO_PROGID;
  }

  if (!CheckBrowserUserChoiceHashes()) {
    LOG_ERROR_MESSAGE(L"UserChoice Hash mismatch");
    return MOZ_E_HASH_CHECK;
  }

  if (!mozilla::IsWin10CreatorsUpdateOrLater()) {
    LOG_ERROR_MESSAGE(L"UserChoice hash matched, but Windows build is too old");
    return MOZ_E_BUILD;
  }

  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return E_FAIL;
  }

  bool ok = true;
  bool defaultRejected = false;

  struct {
    const wchar_t* ext;
    const wchar_t* progID;
  } associations[] = {{L"https", urlProgID.get()},
                      {L"http", urlProgID.get()},
                      {L".html", htmlProgID.get()},
                      {L".htm", htmlProgID.get()}};
  for (size_t i = 0; i < mozilla::ArrayLength(associations); ++i) {
    if (!SetUserChoice(associations[i].ext, sid.get(),
                       associations[i].progID)) {
      ok = false;
      break;
    } else if (!VerifyUserDefault(associations[i].ext,
                                  associations[i].progID)) {
      defaultRejected = true;
      ok = false;
      break;
    }
  }

  if (ok) {
    HRESULT hr = SetDefaultExtensionHandlersUserChoiceImpl(
        aAumi, sid.get(), aExtraFileExtensions);
    if (hr == MOZ_E_REJECTED) {
      ok = false;
      defaultRejected = true;
    } else if (hr == E_FAIL) {
      ok = false;
    }
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  if (!ok) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
    if (defaultRejected) {
      return MOZ_E_REJECTED;
    }
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SetDefaultExtensionHandlersUserChoice(
    const wchar_t* aAumi, const wchar_t* const* aFileExtensions) {
  auto sid = GetCurrentUserStringSid();
  if (!sid) {
    return E_FAIL;
  }

  bool ok = true;
  bool defaultRejected = false;

  HRESULT hr = SetDefaultExtensionHandlersUserChoiceImpl(aAumi, sid.get(),
                                                         aFileExtensions);
  if (hr == MOZ_E_REJECTED) {
    ok = false;
    defaultRejected = true;
  } else if (hr == E_FAIL) {
    ok = false;
  }

  // Notify shell to refresh icons
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  if (!ok) {
    LOG_ERROR_MESSAGE(L"Failed setting default with %s", aAumi);
    if (defaultRejected) {
      return MOZ_E_REJECTED;
    }
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SetDefaultExtensionHandlersUserChoiceImpl(
    const wchar_t* aAumi, const wchar_t* const aSid,
    const wchar_t* const* aFileExtensions) {
  const wchar_t* const* extraFileExtension = aFileExtensions;
  const wchar_t* const* extraProgIDRoot = aFileExtensions + 1;
  while (extraFileExtension && *extraFileExtension && extraProgIDRoot &&
         *extraProgIDRoot) {
    // Formatting the ProgID here prevents using this helper to target arbitrary
    // ProgIDs.
    auto extraProgID = FormatProgID(*extraProgIDRoot, aAumi);

    if (!SetUserChoice(*extraFileExtension, aSid, extraProgID.get())) {
      return E_FAIL;
    }

    if (!VerifyUserDefault(*extraFileExtension, extraProgID.get())) {
      return MOZ_E_REJECTED;
    }

    extraFileExtension += 2;
    extraProgIDRoot += 2;
  }

  return S_OK;
}
