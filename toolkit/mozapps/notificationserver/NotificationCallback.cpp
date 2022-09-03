/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventLog.h"
#include "NotificationCallback.h"

#include <sstream>
#include <string>
#include <vector>

#include "nsWindowsHelpers.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/UniquePtr.h"

#define NOTIFICATION_SERVER_EVENT_TIMEOUT_MS (10 * 1000)

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
  std::wstring windowsTag;

  LOG_ERROR_MESSAGE((L"Invoked with arguments: '%s'"), invokedArgs);

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
    } else if (key == L"windowsTag") {
      windowsTag = value;
    } else if (key == L"action") {
      // Remainder of args are from the Web Notification action, don't parse.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1781929.
      break;
    }
  }

  if (program.empty()) {
    LOG_ERROR_MESSAGE((L"No program; not invoking!"));
    return S_OK;
  }

  path programPath = installDir / program;
  programPath += L".exe";

  std::vector<const wchar_t*> childArgv;
  childArgv.push_back(programPath.c_str());

  if (!profile.empty()) {
    childArgv.push_back(L"--profile");
    childArgv.push_back(profile.c_str());
  } else {
    LOG_ERROR_MESSAGE((L"No profile; invocation will choose default profile"));
  }

  if (!windowsTag.empty()) {
    childArgv.push_back(L"--notification-windowsTag");
    childArgv.push_back(windowsTag.c_str());
  } else {
    LOG_ERROR_MESSAGE((L"No windowsTag; invoking anyway"));
  }

  mozilla::UniquePtr<wchar_t[]> cmdLine(
      mozilla::MakeCommandLine(childArgv.size(), childArgv.data()));

  // This event object will let Firefox notify us when it has handled the
  // notification.
  std::wstring eventName(windowsTag);
  nsAutoHandle event;
  if (!eventName.empty()) {
    event.own(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
  }

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  PROCESS_INFORMATION pi = {0};

  // Runs `{program path} [--profile {profile path}]`.
  CreateProcessW(programPath.c_str(), cmdLine.get(), nullptr, nullptr, false,
                 DETACHED_PROCESS | NORMAL_PRIORITY_CLASS, nullptr, nullptr,
                 &si, &pi);

  LOG_ERROR_MESSAGE((L"Invoked %s"), cmdLine.get());

  if (windowsTag.empty()) {
    return S_OK;
  }

  if (event.get()) {
    LOG_ERROR_MESSAGE((L"Waiting on event with name '%s'"), eventName.c_str());

    DWORD result =
        WaitForSingleObject(event, NOTIFICATION_SERVER_EVENT_TIMEOUT_MS);
    if (result == WAIT_TIMEOUT) {
      LOG_ERROR_MESSAGE(L"Wait timed out");
      return S_OK;
    } else if (result == WAIT_FAILED) {
      LOG_ERROR_MESSAGE((L"Wait failed: %#X"), GetLastError());
      return S_OK;
    } else if (result == WAIT_ABANDONED) {
      LOG_ERROR_MESSAGE((L"Wait abandoned"));
      return S_OK;
    } else {
      LOG_ERROR_MESSAGE((L"Wait succeeded!"));
      return S_OK;
    }
  } else {
    LOG_ERROR_MESSAGE((L"Failed to create event with name '%s'"),
                      eventName.c_str());
  }

  return S_OK;
}
