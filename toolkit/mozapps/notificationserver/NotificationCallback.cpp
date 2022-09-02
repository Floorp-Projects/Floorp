/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationCallback.h"

#include <sstream>
#include <string>

HRESULT STDMETHODCALLTYPE
NotificationCallback::QueryInterface(REFIID riid, void** ppvObject) {
  if (!ppvObject) {
    return E_POINTER;
  }

  *ppvObject = nullptr;

  if (!(riid == guid || riid == __uuidof(INotificationActivationCallback) ||
        riid == __uuidof(IUnknown))) {
    return E_NOINTERFACE;
  }

  AddRef();
  *ppvObject = reinterpret_cast<void*>(this);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE NotificationCallback::Activate(
    LPCWSTR appUserModelId, LPCWSTR invokedArgs,
    const NOTIFICATION_USER_INPUT_DATA* data, ULONG dataCount) {
  std::wstring program;
  std::wstring profile;

  std::wistringstream args(invokedArgs);
  for (std::wstring key, value;
       std::getline(args, key) && std::getline(args, value);) {
    if (key == L"program") {
      // Check executable name to prevent running arbitrary executables.
      if (value == L"" MOZ_APP_NAME) {
        program = value;
      }
    } else if (key == L"profile") {
      profile = value;
    } else if (key == L"action") {
      // Remainder of args are from the Web Notification action, don't parse.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1781929.
      break;
    }
  }

  if (!program.empty()) {
    path programPath = installDir / program;
    programPath += L".exe";

    std::wostringstream runArgs;
    if (!profile.empty()) {
      runArgs << L" --profile \"" << profile << L"\"";
    }

    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION pi = {0};

    // Runs `{program path} [--profile {profile path}]`.
    CreateProcessW(programPath.c_str(), runArgs.str().data(), nullptr, nullptr,
                   false, DETACHED_PROCESS | NORMAL_PRIORITY_CLASS, nullptr,
                   nullptr, &si, &pi);
  }

  return S_OK;
}
