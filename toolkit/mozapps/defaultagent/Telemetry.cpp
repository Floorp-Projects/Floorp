/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"

#include <shlobj.h>
#include <shlwapi.h>

#include <fstream>
#include <string>
#include <unordered_map>

#include "common.h"
#include "EventLog.h"

#include "json/json.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/RefPtr.h"
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

#if !defined(RRF_SUBKEY_WOW6464KEY)
#  define RRF_SUBKEY_WOW6464KEY 0x00010000
#endif  // !defined(RRF_SUBKEY_WOW6464KEY)

using TelemetryFieldResult = mozilla::WindowsErrorResult<std::string>;
using FilePathResult = mozilla::WindowsErrorResult<std::wstring>;

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

static TelemetryFieldResult GetDefaultBrowser() {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }

  // Whatever is handling the HTTP protocol is effectively the default browser.
  wchar_t* rawRegisteredApp;
  hr = pAAR->QueryCurrentDefault(L"http", AT_URLPROTOCOL, AL_EFFECTIVE,
                                 &rawRegisteredApp);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return TelemetryFieldResult(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> registeredApp(
      rawRegisteredApp);

  // This maps a prefix of the AppID string used to register each browser's HTTP
  // handler to a custom string that we'll use to identify that browser in our
  // telemetry ping (which is this function's return value).
  // We're assuming that any UWP app set as the default browser must be Edge.
  const std::unordered_map<std::wstring, std::string> AppIDPrefixes = {
      {L"Firefox", "firefox"},       {L"Chrome", "chrome"}, {L"AppX", "edge"},
      {L"MSEdgeHTM", "edge-chrome"}, {L"IE.", "ie"},        {L"Opera", "opera"},
      {L"Brave", "brave"},
  };

  for (const auto& prefix : AppIDPrefixes) {
    if (!wcsnicmp(registeredApp.get(), prefix.first.c_str(),
                  prefix.first.length())) {
      return prefix.second;
    }
  }

  // The default browser is one that we don't know about.
  return std::string("");
}

static TelemetryFieldResult GetPreviousDefaultBrowser(
    std::string& currentDefault) {
  // This function uses two registry values which store the current and the
  // previous default browser. If the actual current default browser is
  // different from the one we have stored, both values will be updated and the
  // new previous default value reported. Otherwise, we'll just report the
  // existing previous default value.

  // We'll need the currentDefault string in UTF-16 so that we can use it
  // in and around the registry.
  int currentDefaultLen =
      MultiByteToWideChar(CP_UTF8, 0, currentDefault.c_str(), -1, nullptr, 0);
  mozilla::UniquePtr<wchar_t[]> wCurrentDefault =
      mozilla::MakeUnique<wchar_t[]>(currentDefaultLen);
  MultiByteToWideChar(CP_UTF8, 0, currentDefault.c_str(), -1,
                      wCurrentDefault.get(), currentDefaultLen);

  // We don't really need to store these values using names that include the
  // install path, because the default browser is a system (per-user) setting,
  // but we're doing it anyway as a means of avoiding concurrency issues if
  // multiple instances of the task are running at once.
  mozilla::UniquePtr<wchar_t[]> installPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(installPath.get())) {
    LOG_ERROR(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    return std::string("");
  }
  std::wstring currentDefaultRegistryValueName(installPath.get());
  currentDefaultRegistryValueName.append(L"|CurrentDefault");
  std::wstring previousDefaultRegistryValueName(installPath.get());
  previousDefaultRegistryValueName.append(L"|PreviousDefault");

  // First, read the "current default" value that is already stored in the
  // registry, or write a value there if there isn't one.
  wchar_t oldCurrentDefault[MAX_PATH + 1] = L"";
  DWORD regStrLen = MAX_PATH + 1;
  LSTATUS ls =
      RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                   currentDefaultRegistryValueName.c_str(), RRF_RT_REG_SZ,
                   nullptr, &oldCurrentDefault, &regStrLen);
  if (ls != ERROR_SUCCESS) {
    RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                    currentDefaultRegistryValueName.c_str(), REG_SZ,
                    wCurrentDefault.get(), currentDefaultLen * sizeof(wchar_t));
    wcsncpy_s(oldCurrentDefault, MAX_PATH, wCurrentDefault.get(), _TRUNCATE);
  }

  // Repeat the above for the "previous default" value.
  wchar_t oldPreviousDefault[MAX_PATH + 1] = L"";
  regStrLen = MAX_PATH + 1;
  ls = RegGetValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                    previousDefaultRegistryValueName.c_str(), RRF_RT_REG_SZ,
                    nullptr, &oldPreviousDefault, &regStrLen);
  if (ls != ERROR_SUCCESS) {
    RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                    previousDefaultRegistryValueName.c_str(), REG_SZ,
                    wCurrentDefault.get(), currentDefaultLen * sizeof(wchar_t));
    wcsncpy_s(oldPreviousDefault, MAX_PATH, wCurrentDefault.get(), _TRUNCATE);
  }

  // Now, see if the two registry values need to be updated because the actual
  // default browser setting has changed since we last ran.
  std::wstring previousDefault(oldPreviousDefault);
  if (wcsnicmp(oldCurrentDefault, wCurrentDefault.get(), currentDefaultLen)) {
    previousDefault = oldCurrentDefault;

    RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                    previousDefaultRegistryValueName.c_str(), REG_SZ,
                    oldCurrentDefault,
                    (wcslen(oldCurrentDefault) + 1) * sizeof(wchar_t));
    RegSetKeyValueW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME,
                    currentDefaultRegistryValueName.c_str(), REG_SZ,
                    wCurrentDefault.get(), currentDefaultLen * sizeof(wchar_t));
  }

  // We need the previous default string in UTF-8 so we can submit it.
  int previousDefaultLen = WideCharToMultiByte(
      CP_UTF8, 0, previousDefault.c_str(), -1, nullptr, 0, nullptr, nullptr);
  mozilla::UniquePtr<char[]> narrowPreviousDefault =
      mozilla::MakeUnique<char[]>(previousDefaultLen);
  WideCharToMultiByte(CP_UTF8, 0, previousDefault.c_str(), -1,
                      narrowPreviousDefault.get(), previousDefaultLen, nullptr,
                      nullptr);
  return std::string(narrowPreviousDefault.get());
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

static mozilla::WindowsError SendPing(std::string defaultBrowser,
                                      std::string previousDefaultBrowser,
                                      std::string osVersion,
                                      std::string osLocale) {
  // Fill in the ping JSON object.
  Json::Value ping;
  ping["build_channel"] = MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL);
  ping["build_version"] = MOZILLA_VERSION;
  ping["default_browser"] = defaultBrowser;
  ping["previous_default_browser"] = previousDefaultBrowser;
  ping["os_version"] = osVersion;
  ping["os_locale"] = osLocale;

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

HRESULT SendDefaultBrowserPing() {
  TelemetryFieldResult defaultBrowserResult = GetDefaultBrowser();
  if (defaultBrowserResult.isErr()) {
    return defaultBrowserResult.unwrapErr().AsHResult();
  }
  std::string defaultBrowser = defaultBrowserResult.unwrap();

  TelemetryFieldResult previousDefaultBrowserResult =
      GetPreviousDefaultBrowser(defaultBrowser);
  if (previousDefaultBrowserResult.isErr()) {
    return previousDefaultBrowserResult.unwrapErr().AsHResult();
  }
  std::string previousDefaultBrowser = previousDefaultBrowserResult.unwrap();

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

  return SendPing(defaultBrowser, previousDefaultBrowser, osVersion, osLocale)
      .AsHResult();
}
