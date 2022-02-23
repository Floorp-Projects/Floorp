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

using BrowserResult = mozilla::WindowsErrorResult<Browser>;

std::string GetStringForBrowser(Browser browser) {
  switch (browser) {
    case Browser::Firefox:
      return std::string("firefox");
    case Browser::Chrome:
      return std::string("chrome");
    case Browser::EdgeWithEdgeHTML:
      return std::string("edge");
    case Browser::EdgeWithBlink:
      return std::string("edge-chrome");
    case Browser::InternetExplorer:
      return std::string("ie");
    case Browser::Opera:
      return std::string("opera");
    case Browser::Brave:
      return std::string("brave");
    case Browser::Unknown:
      return std::string("");
  }
}

Browser GetBrowserFromString(const std::string& browserString) {
  if (browserString.compare("firefox") == 0) {
    return Browser::Firefox;
  }
  if (browserString.compare("chrome") == 0) {
    return Browser::Chrome;
  }
  if (browserString.compare("edge") == 0) {
    return Browser::EdgeWithEdgeHTML;
  }
  if (browserString.compare("edge-chrome") == 0) {
    return Browser::EdgeWithBlink;
  }
  if (browserString.compare("ie") == 0) {
    return Browser::InternetExplorer;
  }
  if (browserString.compare("opera") == 0) {
    return Browser::Opera;
  }
  if (browserString.compare("brave") == 0) {
    return Browser::Brave;
  }
  return Browser::Unknown;
}

static BrowserResult GetDefaultBrowser() {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return BrowserResult(mozilla::WindowsError::FromHResult(hr));
  }

  // Whatever is handling the HTTP protocol is effectively the default browser.
  wchar_t* rawRegisteredApp;
  hr = pAAR->QueryCurrentDefault(L"http", AT_URLPROTOCOL, AL_EFFECTIVE,
                                 &rawRegisteredApp);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return BrowserResult(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> registeredApp(
      rawRegisteredApp);

  // This maps a prefix of the AppID string used to register each browser's HTTP
  // handler to a custom string that we'll use to identify that browser in our
  // telemetry ping (which is this function's return value).
  // We're assuming that any UWP app set as the default browser must be Edge.
  const std::unordered_map<std::wstring, Browser> AppIDPrefixes = {
      {L"Firefox", Browser::Firefox},
      {L"Chrome", Browser::Chrome},
      {L"AppX", Browser::EdgeWithEdgeHTML},
      {L"MSEdgeHTM", Browser::EdgeWithBlink},
      {L"IE.", Browser::InternetExplorer},
      {L"Opera", Browser::Opera},
      {L"Brave", Browser::Brave},
  };

  for (const auto& prefix : AppIDPrefixes) {
    if (!wcsnicmp(registeredApp.get(), prefix.first.c_str(),
                  prefix.first.length())) {
      return prefix.second;
    }
  }

  // The default browser is one that we don't know about.
  return Browser::Unknown;
}

static BrowserResult GetPreviousDefaultBrowser(Browser currentDefault) {
  // This function uses a registry value which stores the current default
  // browser. It returns the data stored in that registry value and replaces the
  // stored string with the current default browser string that was passed in.

  std::string currentDefaultStr = GetStringForBrowser(currentDefault);
  std::string previousDefault =
      RegistryGetValueString(IsPrefixed::Unprefixed, L"CurrentDefault")
          .unwrapOr(mozilla::Some(currentDefaultStr))
          .valueOr(currentDefaultStr);

  mozilla::Unused << RegistrySetValueString(
      IsPrefixed::Unprefixed, L"CurrentDefault", currentDefaultStr.c_str());

  return GetBrowserFromString(previousDefault);
}

DefaultBrowserResult GetDefaultBrowserInfo() {
  DefaultBrowserInfo browserInfo;

  BrowserResult defaultBrowserResult = GetDefaultBrowser();
  if (defaultBrowserResult.isErr()) {
    return DefaultBrowserResult(defaultBrowserResult.unwrapErr());
  }
  browserInfo.currentDefaultBrowser = defaultBrowserResult.unwrap();

  BrowserResult previousDefaultBrowserResult =
      GetPreviousDefaultBrowser(browserInfo.currentDefaultBrowser);
  if (previousDefaultBrowserResult.isErr()) {
    return DefaultBrowserResult(previousDefaultBrowserResult.unwrapErr());
  }
  browserInfo.previousDefaultBrowser = previousDefaultBrowserResult.unwrap();

  return browserInfo;
}

// We used to prefix this key with the installation directory, but that causes
// problems with our new "only one ping per day across installs" restriction.
// To make sure all installations use consistent data, the value's name is
// being migrated to a shared, non-prefixed name.
// This function doesn't really do any error handling, because there isn't
// really anything to be done if it fails.
void MaybeMigrateCurrentDefault() {
  const wchar_t* valueName = L"CurrentDefault";

  MaybeStringResult valueResult =
      RegistryGetValueString(IsPrefixed::Prefixed, valueName);
  if (valueResult.isErr()) {
    return;
  }
  mozilla::Maybe<std::string> maybeValue = valueResult.unwrap();
  if (maybeValue.isNothing()) {
    // No value to migrate
    return;
  }
  std::string value = maybeValue.value();

  mozilla::Unused << RegistryDeleteValue(IsPrefixed::Prefixed, valueName);

  // Only migrate the value if no value is in the new location yet.
  valueResult = RegistryGetValueString(IsPrefixed::Unprefixed, valueName);
  if (valueResult.isErr()) {
    return;
  }
  if (valueResult.unwrap().isNothing()) {
    mozilla::Unused << RegistrySetValueString(IsPrefixed::Unprefixed, valueName,
                                              value.c_str());
  }
}
