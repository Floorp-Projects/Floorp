/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DefaultBrowser.h"

#include <string>
#include <unordered_map>

#include <shlobj.h>
#include <shlwapi.h>

#include "common.h"
#include "EventLog.h"
#include "Registry.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WinHeaderOnlyUtils.h"

using StringResult = mozilla::WindowsErrorResult<std::string>;

static StringResult GetDefaultBrowser() {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return StringResult(mozilla::WindowsError::FromHResult(hr));
  }

  // Whatever is handling the HTTP protocol is effectively the default browser.
  wchar_t* rawRegisteredApp;
  hr = pAAR->QueryCurrentDefault(L"http", AT_URLPROTOCOL, AL_EFFECTIVE,
                                 &rawRegisteredApp);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return StringResult(mozilla::WindowsError::FromHResult(hr));
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

static StringResult GetPreviousDefaultBrowser(
    const std::string& currentDefault) {
  // This function uses a registry value which stores the current default
  // browser. It returns the data stored in that registry value and replaces the
  // stored string with the current default browser string that was passed in.

  std::string previousDefault =
      RegistryGetValueString(IsPrefixed::Prefixed, L"CurrentDefault")
          .unwrapOr(mozilla::Some(currentDefault))
          .valueOr(currentDefault);

  mozilla::Unused << RegistrySetValueString(
      IsPrefixed::Prefixed, L"CurrentDefault", currentDefault.c_str());

  return previousDefault;
}

DefaultBrowserResult GetDefaultBrowserInfo() {
  DefaultBrowserInfo browserInfo;

  StringResult defaultBrowserResult = GetDefaultBrowser();
  if (defaultBrowserResult.isErr()) {
    return DefaultBrowserResult(defaultBrowserResult.unwrapErr());
  }
  browserInfo.currentDefaultBrowser = defaultBrowserResult.unwrap();

  StringResult previousDefaultBrowserResult =
      GetPreviousDefaultBrowser(browserInfo.currentDefaultBrowser);
  if (previousDefaultBrowserResult.isErr()) {
    return DefaultBrowserResult(previousDefaultBrowserResult.unwrapErr());
  }
  browserInfo.previousDefaultBrowser = previousDefaultBrowserResult.unwrap();

  return browserInfo;
}
