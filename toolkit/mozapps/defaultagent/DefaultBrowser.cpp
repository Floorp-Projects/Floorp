/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DefaultBrowser.h"

#include <string>

#include <shlobj.h>

#include "EventLog.h"
#include "Registry.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Try.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

using BrowserResult = mozilla::WindowsErrorResult<Browser>;

constexpr std::string_view kUnknownBrowserString = "";

constexpr std::pair<std::string_view, Browser> kStringBrowserMap[]{
    {"error", Browser::Error},
    {kUnknownBrowserString, Browser::Unknown},
    {"firefox", Browser::Firefox},
    {"chrome", Browser::Chrome},
    {"edge", Browser::EdgeWithEdgeHTML},
    {"edge-chrome", Browser::EdgeWithBlink},
    {"ie", Browser::InternetExplorer},
    {"opera", Browser::Opera},
    {"brave", Browser::Brave},
    {"yandex", Browser::Yandex},
    {"qq-browser", Browser::QQBrowser},
    {"360-browser", Browser::_360Browser},
    {"sogou", Browser::Sogou},
    {"duckduckgo", Browser::DuckDuckGo},
};

static_assert(mozilla::ArrayLength(kStringBrowserMap) == kBrowserCount);

std::string GetStringForBrowser(Browser browser) {
  for (const auto& [mapString, mapBrowser] : kStringBrowserMap) {
    if (browser == mapBrowser) {
      return std::string{mapString};
    }
  }

  return std::string(kUnknownBrowserString);
}

Browser GetBrowserFromString(const std::string& browserString) {
  for (const auto& [mapString, mapBrowser] : kStringBrowserMap) {
    if (browserString == mapString) {
      return mapBrowser;
    }
  }

  return Browser::Unknown;
}

BrowserResult TryGetDefaultBrowser() {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return BrowserResult(mozilla::WindowsError::FromHResult(hr));
  }

  // Whatever is handling the HTTP protocol is effectively the default browser.
  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> registeredApp;
  {
    wchar_t* rawRegisteredApp;
    hr = pAAR->QueryCurrentDefault(L"http", AT_URLPROTOCOL, AL_EFFECTIVE,
                                   &rawRegisteredApp);
    if (FAILED(hr)) {
      LOG_ERROR(hr);
      return BrowserResult(mozilla::WindowsError::FromHResult(hr));
    }
    registeredApp = mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter>(
        rawRegisteredApp);
  }

  // Get the application Friendly Name associated to the found ProgID. This is
  // sized to be larger than any observed or expected friendly names. Long
  // friendly names tend to be in the form `[Company] [Browser] [Variant]`
  std::array<wchar_t, 256> friendlyName{};
  DWORD friendlyNameLen = friendlyName.size();
  hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_FRIENDLYAPPNAME,
                         registeredApp.get(), nullptr, friendlyName.data(),
                         &friendlyNameLen);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return BrowserResult(mozilla::WindowsError::FromHResult(hr));
  }

  // This maps a browser's Friendly Name prefix to an enum variant that we'll
  // use to identify that browser in our telemetry ping (which is this
  // function's return value).
  constexpr std::pair<std::wstring_view, Browser> kFriendlyNamePrefixes[] = {
      {L"Firefox", Browser::Firefox},
      {L"Google Chrome", Browser::Chrome},
      {L"Microsoft Edge", Browser::EdgeWithBlink},
      {L"Internet Explorer", Browser::InternetExplorer},
      {L"Opera", Browser::Opera},
      {L"Brave", Browser::Brave},
      {L"Yandex", Browser::Yandex},
      {L"QQBrowser", Browser::QQBrowser},
      // 360安全浏览器 UTF-16 encoding
      {L"\u0033\u0036\u0030\u5b89\u5168\u6d4f\u89c8\u5668",
       Browser::_360Browser},
      // 搜狗高速浏览器 UTF-16 encoding
      {L"\u641c\u72d7\u9ad8\u901f\u6d4f\u89c8\u5668", Browser::Sogou},
      {L"DuckDuckGo", Browser::DuckDuckGo},
  };

  // We should have one prefix for every browser we track, minus exceptions
  // listed below.
  // Error - not a real browser.
  // Unknown - not a real browser.
  // EdgeWithEdgeHTML - duplicate friendly name with EdgeWithBlink with special
  //   handling below.
  static_assert(mozilla::ArrayLength(kFriendlyNamePrefixes) ==
                kBrowserCount - 3);

  for (const auto& [prefix, browser] : kFriendlyNamePrefixes) {
    // Find matching Friendly Name prefix.
    if (!wcsnicmp(friendlyName.data(), prefix.data(), prefix.length())) {
      if (browser == Browser::EdgeWithBlink) {
        // Disambiguate EdgeWithEdgeHTML and EdgeWithBlink.
        // The ProgID below is documented as having not changed while Edge was
        // actively developed. It's assumed but unverified this is true in all
        // cases (e.g. across locales).
        //
        // Note: at time of commit EdgeWithBlink from the Windows Store was a
        // wrapper for Edge Installer instead of a package containing Edge,
        // therefore the Default Browser associating ProgID was not in the form
        // "AppX[hash]" as expected. It is unclear if the EdgeWithEdgeHTML and
        // EdgeWithBlink ProgIDs would differ if the latter is changed into a
        // package containing Edge.
        constexpr std::wstring_view progIdEdgeHtml1{
            L"AppXq0fevzme2pys62n3e0fbqa7peapykr8v"};
        // Apparently there is at least one other ProgID used by EdgeHTML Edge.
        constexpr std::wstring_view progIdEdgeHtml2{
            L"AppXd4nrz8ff68srnhf9t5a8sbjyar1cr723"};

        if (!wcsnicmp(registeredApp.get(), progIdEdgeHtml1.data(),
                      progIdEdgeHtml1.length()) ||
            !wcsnicmp(registeredApp.get(), progIdEdgeHtml2.data(),
                      progIdEdgeHtml2.length())) {
          return Browser::EdgeWithEdgeHTML;
        }
      }

      return browser;
    }
  }

  // The default browser is one that we don't know about.
  return Browser::Unknown;
}

BrowserResult TryGetReplacePreviousDefaultBrowser(Browser currentDefault) {
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

  MOZ_TRY_VAR(browserInfo.currentDefaultBrowser, TryGetDefaultBrowser());
  MOZ_TRY_VAR(
      browserInfo.previousDefaultBrowser,
      TryGetReplacePreviousDefaultBrowser(browserInfo.currentDefaultBrowser));

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

Browser GetDefaultBrowser() {
  return TryGetDefaultBrowser().unwrapOr(Browser::Error);
}
Browser GetReplacePreviousDefaultBrowser(Browser currentBrowser) {
  return TryGetReplacePreviousDefaultBrowser(currentBrowser)
      .unwrapOr(Browser::Error);
}

}  // namespace mozilla::default_agent
