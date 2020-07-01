/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"

#include <fstream>
#include <string>

#include <windows.h>

#include "common.h"
#include "EventLog.h"

#include "json/json.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#define TELEMETRY_BASE_URL "https://incoming.telemetry.mozilla.org/submit"
#define TELEMETRY_NAMESPACE "default-browser-agent"
#define TELEMETRY_PING_VERSION "1"
#define TELEMETRY_PING_DOCTYPE "default-browser"

// This is almost the complete URL, just needs a UUID appended.
#define TELEMETRY_PING_URL                                              \
  TELEMETRY_BASE_URL "/" TELEMETRY_NAMESPACE "/" TELEMETRY_PING_DOCTYPE \
                     "/" TELEMETRY_PING_VERSION "/"

// We only want to send one ping per day. However, this is slightly less than 24
// hours so that we have a little bit of wiggle room on our task, which is also
// supposed to run every 24 hours.
#define MINIMUM_PING_PERIOD_SEC ((23 * 60 * 60) + (45 * 60))

#if !defined(RRF_SUBKEY_WOW6464KEY)
#  define RRF_SUBKEY_WOW6464KEY 0x00010000
#endif  // !defined(RRF_SUBKEY_WOW6464KEY)

using TelemetryFieldResult = mozilla::WindowsErrorResult<std::string>;
using FilePathResult = mozilla::WindowsErrorResult<std::wstring>;
using BoolResult = mozilla::WindowsErrorResult<bool>;

// This function was copied from the implementation of
// nsITelemetry::isOfficialTelemetry, currently found in the file
// toolkit/components/telemetry/core/Telemetry.cpp.
static bool IsOfficialTelemetry() {
#if defined(MOZILLA_OFFICIAL) && defined(MOZ_TELEMETRY_REPORTING) && \
    !defined(DEBUG)
  return true;
#else
  return false;
#endif
}

static TelemetryFieldResult GetOSVersion() {
  OSVERSIONINFOEXW osv = {sizeof(osv)};
  if (::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osv))) {
    std::ostringstream oss;
    oss << osv.dwMajorVersion << "." << osv.dwMinorVersion << "."
        << osv.dwBuildNumber;

    if (osv.dwMajorVersion == 10 && osv.dwMinorVersion == 0) {
      // Get the "Update Build Revision" (UBR) value
      DWORD ubrValue;
      DWORD ubrValueLen = sizeof(ubrValue);
      LSTATUS ubrOk =
          ::RegGetValueW(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                         L"UBR", RRF_RT_DWORD | RRF_SUBKEY_WOW6464KEY, nullptr,
                         &ubrValue, &ubrValueLen);
      if (ubrOk == ERROR_SUCCESS) {
        oss << "." << ubrValue;
      }
    }

    return oss.str();
  }

  HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
  LOG_ERROR(hr);
  return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
}

static TelemetryFieldResult GetOSLocale() {
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = L"";
  if (!GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }

  // We'll need the locale string in UTF-8 to be able to submit it.
  int bufLen = WideCharToMultiByte(CP_UTF8, 0, localeName, -1, nullptr, 0,
                                   nullptr, nullptr);
  mozilla::UniquePtr<char[]> narrowLocaleName =
      mozilla::MakeUnique<char[]>(bufLen);
  if (!narrowLocaleName) {
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    LOG_ERROR(hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }
  WideCharToMultiByte(CP_UTF8, 0, localeName, -1, narrowLocaleName.get(),
                      bufLen, nullptr, nullptr);

  return std::string(narrowLocaleName.get());
}

static FilePathResult GenerateUUIDStr() {
  UUID uuid;
  RPC_STATUS status = UuidCreate(&uuid);
  if (status != RPC_S_OK) {
    HRESULT hr = MAKE_HRESULT(1, FACILITY_RPC, status);
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  // 39 == length of a UUID string including braces and NUL.
  wchar_t guidBuf[39] = {};
  if (StringFromGUID2(uuid, guidBuf, 39) != 39) {
    LOG_ERROR(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    return FilePathResult(
        mozilla::WindowsError::FromWin32Error(ERROR_INSUFFICIENT_BUFFER));
  }

  // Remove the curly braces.
  return std::wstring(guidBuf + 1, guidBuf + 37);
}

static FilePathResult GetPingFilePath(std::wstring& uuid) {
  wchar_t* rawAppDataPath;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr,
                                    &rawAppDataPath);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> appDataPath(
      rawAppDataPath);

  // The Path* functions don't set LastError, but this is the only thing that
  // can really cause them to fail, so if they ever do we assume this is why.
  hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

  wchar_t pingFilePath[MAX_PATH] = L"";
  if (!PathCombineW(pingFilePath, appDataPath.get(), L"" MOZ_APP_VENDOR)) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  if (!PathAppendW(pingFilePath, L"" MOZ_APP_BASENAME)) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  if (!PathAppendW(pingFilePath, L"Pending Pings")) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  if (!PathAppendW(pingFilePath, uuid.c_str())) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  return std::wstring(pingFilePath);
}

static FilePathResult GetPingsenderPath() {
  // The Path* functions don't set LastError, but this is the only thing that
  // can really cause them to fail, so if they ever do we assume this is why.
  HRESULT hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

  mozilla::UniquePtr<wchar_t[]> thisBinaryPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(thisBinaryPath.get())) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  wchar_t pingsenderPath[MAX_PATH] = L"";

  if (!PathCombineW(pingsenderPath, thisBinaryPath.get(), L"pingsender.exe")) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  return std::wstring(pingsenderPath);
}

static mozilla::WindowsError SendPing(const std::string defaultBrowser,
                                      const std::string previousDefaultBrowser,
                                      const std::string osVersion,
                                      const std::string osLocale,
                                      const std::string notificationType,
                                      const std::string notificationShown,
                                      const std::string notificationAction) {
  // Fill in the ping JSON object.
  Json::Value ping;
  ping["build_channel"] = MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL);
  ping["build_version"] = MOZILLA_VERSION;
  ping["default_browser"] = defaultBrowser;
  ping["previous_default_browser"] = previousDefaultBrowser;
  ping["os_version"] = osVersion;
  ping["os_locale"] = osLocale;
  ping["notification_type"] = notificationType;
  ping["notification_shown"] = notificationShown;
  ping["notification_action"] = notificationAction;

  // Stringify the JSON.
  Json::StreamWriterBuilder jsonStream;
  jsonStream["indentation"] = "";
  std::string pingStr = Json::writeString(jsonStream, ping);

  // Generate a UUID for the ping.
  FilePathResult uuidResult = GenerateUUIDStr();
  if (uuidResult.isErr()) {
    return uuidResult.unwrapErr();
  }
  std::wstring uuid = uuidResult.unwrap();

  // Write the JSON string to a file. Use the UUID in the file name so that if
  // multiple instances of this task are running they'll have their own files.
  FilePathResult pingFilePathResult = GetPingFilePath(uuid);
  if (pingFilePathResult.isErr()) {
    return pingFilePathResult.unwrapErr();
  }
  std::wstring pingFilePath = pingFilePathResult.unwrap();

  {
    std::ofstream outFile(pingFilePath);
    outFile << pingStr;
    if (outFile.fail()) {
      // We have no way to get a specific error code out of a file stream
      // other than to catch an exception, so substitute a generic error code.
      HRESULT hr = HRESULT_FROM_WIN32(ERROR_IO_DEVICE);
      LOG_ERROR(hr);
      return mozilla::WindowsError::FromHResult(hr);
    }
  }

  // Hand the file off to pingsender to submit.
  FilePathResult pingsenderPathResult = GetPingsenderPath();
  if (pingsenderPathResult.isErr()) {
    return pingsenderPathResult.unwrapErr();
  }
  std::wstring pingsenderPath = pingsenderPathResult.unwrap();

  std::wstring url(L"" TELEMETRY_PING_URL);
  url.append(uuid);

  const wchar_t* pingsenderArgs[] = {pingsenderPath.c_str(), url.c_str(),
                                     pingFilePath.c_str()};
  mozilla::UniquePtr<wchar_t[]> pingsenderCmdLine(
      mozilla::MakeCommandLine(mozilla::ArrayLength(pingsenderArgs),
                               const_cast<wchar_t**>(pingsenderArgs)));

  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  if (!::CreateProcessW(pingsenderPath.c_str(), pingsenderCmdLine.get(),
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                        &pi)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::WindowsError::FromHResult(hr);
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return mozilla::WindowsError::CreateSuccess();
}

// This function checks if a ping has already been sent today. If one has not,
// it assumes that we are about to send one and sets a registry entry that will
// cause this function to return true for the next day.
// This function uses unprefixed registry entries, so a RegistryMutex should be
// held before calling.
static BoolResult GetPingAlreadySentToday() {
  const wchar_t* valueName = L"LastPingSentAt";
  MaybeQwordResult readResult =
      RegistryGetValueQword(IsPrefixed::Unprefixed, valueName);
  if (readResult.isErr()) {
    HRESULT hr = readResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Unable to read registry: %#X", hr);
    return BoolResult(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::Maybe<ULONGLONG> maybeValue = readResult.unwrap();
  ULONGLONG now = GetCurrentTimestamp();
  if (maybeValue.isSome()) {
    ULONGLONG lastPingTime = maybeValue.value();
    if (SecondsPassedSince(lastPingTime, now) < MINIMUM_PING_PERIOD_SEC) {
      return true;
    }
  }

  mozilla::WindowsErrorResult<mozilla::Ok> writeResult =
      RegistrySetValueQword(IsPrefixed::Unprefixed, valueName, now);
  if (writeResult.isErr()) {
    HRESULT hr = readResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Unable to write registry: %#X", hr);
    return BoolResult(mozilla::WindowsError::FromHResult(hr));
  }
  return false;
}

// This both retrieves a value from the registry and writes new data
// (currentDefault) to the same value. If there is no value stored, the value
// passed for prevDefault will be converted to a string and returned instead.
//
// Although we already store and retrieve a cached previous default browser
// value elsewhere, it may be updated when we don't send a ping. The value we
// retrieve here will only be updated when we are sending a ping to ensure
// that pings don't miss a default browser transition.
static TelemetryFieldResult GetAndUpdatePreviousDefaultBrowser(
    const std::string& currentDefault, Browser prevDefault) {
  const wchar_t* registryValueName = L"PingCurrentDefault";

  MaybeStringResult readResult =
      RegistryGetValueString(IsPrefixed::Unprefixed, registryValueName);
  if (readResult.isErr()) {
    HRESULT hr = readResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Unable to read registry: %#X", hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::Maybe<std::string> maybeValue = readResult.unwrap();
  std::string oldCurrentDefault;
  if (maybeValue.isSome()) {
    oldCurrentDefault = maybeValue.value();
  } else {
    oldCurrentDefault = GetStringForBrowser(prevDefault);
  }

  mozilla::WindowsErrorResult<mozilla::Ok> writeResult = RegistrySetValueString(
      IsPrefixed::Unprefixed, registryValueName, currentDefault.c_str());
  if (writeResult.isErr()) {
    HRESULT hr = readResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Unable to write registry: %#X", hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }
  return oldCurrentDefault;
}

HRESULT SendDefaultBrowserPing(
    const DefaultBrowserInfo& browserInfo,
    const NotificationActivities& activitiesPerformed) {
  std::string currentDefaultBrowser =
      GetStringForBrowser(browserInfo.currentDefaultBrowser);
  std::string notificationType =
      GetStringForNotificationType(activitiesPerformed.type);
  std::string notificationShown =
      GetStringForNotificationShown(activitiesPerformed.shown);
  std::string notificationAction =
      GetStringForNotificationAction(activitiesPerformed.action);

  TelemetryFieldResult osVersionResult = GetOSVersion();
  if (osVersionResult.isErr()) {
    return osVersionResult.unwrapErr().AsHResult();
  }
  std::string osVersion = osVersionResult.unwrap();

  TelemetryFieldResult osLocaleResult = GetOSLocale();
  if (osLocaleResult.isErr()) {
    return osLocaleResult.unwrapErr().AsHResult();
  }
  std::string osLocale = osLocaleResult.unwrap();

  // Do not send the ping if we are not an official telemetry-enabled build;
  // don't even generate the ping in fact, because if we write the file out
  // then some other build might find it later and decide to submit it.
  if (!IsOfficialTelemetry()) {
    return S_OK;
  }

  // Pings are limited to one per day (across all installations), so check if we
  // already sent one today.
  // This will also set a registry entry indicating that the last ping was
  // just sent, to prevent another one from being sent today. We'll do this
  // now even though we haven't sent the ping yet. After this check, we send
  // a ping unconditionally. The only exception is for errors, and any error
  // that we get now will probably be hit every time.
  // Because unsent pings attempted with pingsender can get automatically
  // re-sent later, we don't even want to try again on transient network
  // failures.
  BoolResult pingAlreadySentResult = GetPingAlreadySentToday();
  if (pingAlreadySentResult.isErr()) {
    return pingAlreadySentResult.unwrapErr().AsHResult();
  }
  bool pingAlreadySent = pingAlreadySentResult.unwrap();
  if (pingAlreadySent) {
    return S_OK;
  }

  // Don't update the registry's default browser data until we are sure we
  // want to send a ping. Otherwise it could be updated to reflect a ping we
  // never sent.
  TelemetryFieldResult previousDefaultBrowserResult =
      GetAndUpdatePreviousDefaultBrowser(currentDefaultBrowser,
                                         browserInfo.previousDefaultBrowser);
  if (previousDefaultBrowserResult.isErr()) {
    return previousDefaultBrowserResult.unwrapErr().AsHResult();
  }
  std::string previousDefaultBrowser = previousDefaultBrowserResult.unwrap();

  return SendPing(currentDefaultBrowser, previousDefaultBrowser, osVersion,
                  osLocale, notificationType, notificationShown,
                  notificationAction)
      .AsHResult();
}
