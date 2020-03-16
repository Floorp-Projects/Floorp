/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shlwapi.h>
#include <objbase.h>
#include <string.h>

#include "nsAutoRef.h"
#include "nsWindowsHelpers.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#include "ScheduledTask.h"
#include "Policy.h"
#include "Telemetry.h"

static void RemoveAllRegistryEntries() {
  mozilla::UniquePtr<wchar_t[]> installPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(installPath.get())) {
    return;
  }

  const wchar_t* regKeyName = L"SOFTWARE\\" MOZ_APP_VENDOR "\\" MOZ_APP_BASENAME
                              "\\Default Browser Agent";

  HKEY rawRegKey = nullptr;
  if (ERROR_SUCCESS !=
      RegOpenKeyExW(HKEY_CURRENT_USER, regKeyName, 0,
                    KEY_WRITE | KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                    &rawRegKey)) {
    return;
  }
  nsAutoRegKey regKey(rawRegKey);

  DWORD maxValueNameLen = 0;
  if (ERROR_SUCCESS != RegQueryInfoKeyW(regKey.get(), nullptr, nullptr, nullptr,
                                        nullptr, nullptr, nullptr, nullptr,
                                        &maxValueNameLen, nullptr, nullptr,
                                        nullptr)) {
    return;
  }
  // The length that RegQueryInfoKeyW returns is without a terminator.
  maxValueNameLen += 1;

  mozilla::UniquePtr<wchar_t[]> valueName =
      mozilla::MakeUnique<wchar_t[]>(maxValueNameLen);

  DWORD valueIndex = 0;

  while (true) {
    DWORD valueNameLen = maxValueNameLen;
    LSTATUS ls =
        RegEnumValueW(regKey.get(), valueIndex, valueName.get(), &valueNameLen,
                      nullptr, nullptr, nullptr, nullptr);
    if (ls != ERROR_SUCCESS) {
      break;
    }

    if (!wcsnicmp(valueName.get(), installPath.get(),
                  wcslen(installPath.get()))) {
      RegDeleteValue(regKey.get(), valueName.get());
      // Only increment the index if we did not delete this value, because if
      // we did then the indexes of all the values after that one just got
      // decremented, meaning the index we already have now refers to a value
      // that we haven't looked at yet.
    } else {
      valueIndex++;
    }
  }

  // If we just deleted every value, then also delete the key.
  if (ERROR_SUCCESS != RegQueryInfoKeyW(regKey.get(), nullptr, nullptr, nullptr,
                                        nullptr, nullptr, nullptr, &valueIndex,
                                        nullptr, nullptr, nullptr, nullptr)) {
    return;
  }

  regKey.reset();

  if (valueIndex == 0) {
    RegDeleteKeyW(HKEY_CURRENT_USER, regKeyName);
  }
}

// We expect to be given a command string in argv[1], perhaps followed by other
// arguments depending on the command. The valid commands are:
// register-task [unique-token]
//   Create a Windows scheduled task that will launch this binary with the
//   do-task command every 24 hours, starting from 24 hours after register-task
//   is run. unique-token is required and should be some string that uniquely
//   identifies this installation of the product; typically this will be the
//   install path hash that's used for the update directory, the AppUserModelID,
//   and other related purposes.
// update-task [unique-token]
//   Update an existing task registration, without changing its schedule. This
//   should be called during updates of the application, in case this program
//   has been updated and any of the task parameters have changed. The unique
//   token argument is required and should be the same one that was passed in
//   when the task was registered.
// unregister-task [unique-token]
//   Removes the previously created task along with any registry entries that
//   running the task may have created. The unique token argument is required
//   and should be the same one that was passed in when the task was registered.
// do-task
//   Actually performs the default agent task, which currently means generating
//   and sending our telemetry ping.
int wmain(int argc, wchar_t** argv) {
  if (argc < 2 || !argv[1]) {
    return E_INVALIDARG;
  }

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return hr;
  }

  const struct ComUninitializer {
    ~ComUninitializer() { CoUninitialize(); }
  } kCUi;

  // The remove-task command is allowed even if the policy disabling the task
  // is set, mainly so that the uninstaller will work.
  if (!wcscmp(argv[1], L"unregister-task")) {
    if (argc < 3 || !argv[2]) {
      return E_INVALIDARG;
    }
    RemoveAllRegistryEntries();
    return RemoveTask(argv[2]);
  }

  if (IsAgentDisabled()) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY);
  }

  if (!wcscmp(argv[1], L"register-task")) {
    if (argc < 3 || !argv[2]) {
      return E_INVALIDARG;
    }
    return RegisterTask(argv[2]);
  } else if (!wcscmp(argv[1], L"update-task")) {
    if (argc < 3 || !argv[2]) {
      return E_INVALIDARG;
    }
    return UpdateTask(argv[2]);
  } else if (!wcscmp(argv[1], L"do-task")) {
    if (!IsTelemetryDisabled()) {
      return SendDefaultBrowserPing();
    }
    return S_OK;
  } else {
    return E_INVALIDARG;
  }
}
