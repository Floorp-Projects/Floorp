/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteSettings.h"

#include <iostream>
#include <windows.h>
#include <shlwapi.h>

#include "common.h"
#include "Registry.h"

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Unused.h"

// All strings cross the C/C++ <-> Rust FFI boundary as
// null-terminated UTF-8.
extern "C" {
HRESULT IsAgentRemoteDisabledRust(const char* szUrl, DWORD* lpdwDisabled);
}

#define PROD_ENDPOINT "https://firefox.settings.services.mozilla.com/v1"
#define PROD_BID "main"
#define PROD_CID "windows-default-browser-agent"
#define PROD_ID "state"

#define PATH "buckets/" PROD_BID "/collections/" PROD_CID "/records/" PROD_ID

using BoolResult = mozilla::WindowsErrorResult<bool>;

// Use Rust library to query remote service endpoint to determine if
// WDBA is disabled.
//
// Pass through errors.
static BoolResult IsAgentRemoteDisabledInternal() {
  // Fetch remote settings server root from registry.
  auto serverResult =
      RegistryGetValueString(IsPrefixed::Prefixed, L"ServicesSettingsServer");

  // Unconfigured?  Fallback to production.
  std::string url =
      serverResult.unwrapOr(mozilla::Some(std::string(PROD_ENDPOINT)))
          .valueOr(std::string(PROD_ENDPOINT));

  if (url.length() > 0 && url[url.length() - 1] != '/') {
    url += '/';
  }
  url += PATH;

  std::cerr << "default-browser-agent: Remote service disabled state URL: '"
            << url << "'" << std::endl;

  // Invoke Rust to query remote settings.
  DWORD isRemoteDisabled;
  HRESULT hr = IsAgentRemoteDisabledRust(url.c_str(), &isRemoteDisabled);

  std::cerr << "default-browser-agent: HRESULT: 0x" << std::hex << hr
            << std::endl;

  if (SUCCEEDED(hr)) {
    return (0 != isRemoteDisabled);
  }

  LOG_ERROR(hr);
  return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
}

// Query remote service endpoint to determine if WDBA is disabled.
//
// Handle errors, failing to the last state witnessed without error.
bool IsAgentRemoteDisabled() {
  // Fetch last witnessed state from registry.  If we can't get the last
  // state, or there is no last state, assume we're not disabled.
  bool lastRemoteDisabled =
      RegistryGetValueBool(IsPrefixed::Prefixed,
                           L"DefaultAgentLastRemoteDisabled")
          .unwrapOr(mozilla::Some(false))
          .valueOr(false);

  std::cerr << "default-browser-agent: Last remote disabled: "
            << lastRemoteDisabled << std::endl;

  auto remoteDisabledResult = IsAgentRemoteDisabledInternal();
  if (remoteDisabledResult.isErr()) {
    // Fail to the last state witnessed without error.
    return lastRemoteDisabled;
  }

  bool remoteDisabled = remoteDisabledResult.unwrap();

  std::cerr << "default-browser-agent: Next remote disabled: " << remoteDisabled
            << std::endl;

  // Update last witnessed state in registry.
  mozilla::Unused << RegistrySetValueBool(
      IsPrefixed::Prefixed, L"DefaultAgentLastRemoteDisabled", remoteDisabled);

  return remoteDisabled;
}
