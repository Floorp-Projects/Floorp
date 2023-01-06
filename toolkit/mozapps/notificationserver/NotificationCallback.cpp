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

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/ToastNotificationHeaderOnlyUtils.h"

#define NOTIFICATION_SERVER_EVENT_TIMEOUT_MS (10 * 1000)
using namespace mozilla::widget::toastnotification;

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
  HandleActivation(invokedArgs);

  // Windows 8 style callbacks are not called and notifications are not removed
  // from the Action Center unless we return `S_OK`, so always do so even if
  // we're unable to handle the notification properly.
  return S_OK;
}

void NotificationCallback::HandleActivation(LPCWSTR invokedArgs) {
  LOG_ERROR_MESSAGE(L"Invoked with arguments: '%s'", invokedArgs);

  auto maybeArgs = ParseToastArguments(invokedArgs);
  if (!maybeArgs) {
    LOG_ERROR_MESSAGE(L"COM server disabled for toast");
    return;
  }
  const auto& args = maybeArgs.value();
  auto [programPath, cmdLine] = BuildRunCommand(args);

  // This event object will let Firefox notify us when it has handled the
  // notification.
  std::wstring eventName(args.windowsTag);
  nsAutoHandle event;
  if (!eventName.empty()) {
    event.own(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
  }

  // Run the application.

  STARTUPINFOW si = {};
  si.cb = sizeof(STARTUPINFOW);
  PROCESS_INFORMATION pi = {};

  // Runs `{program path} [--profile {profile path}] [--notification-windowsTag
  // {tag}]`.
  CreateProcessW(programPath.c_str(), cmdLine.get(), nullptr, nullptr, false,
                 DETACHED_PROCESS | NORMAL_PRIORITY_CLASS, nullptr, nullptr,
                 &si, &pi);

  LOG_ERROR_MESSAGE(L"Invoked %s", cmdLine.get());

  if (event.get()) {
    LOG_ERROR_MESSAGE((L"Waiting on event with name '%s'"), eventName.c_str());

    DWORD result =
        WaitForSingleObject(event, NOTIFICATION_SERVER_EVENT_TIMEOUT_MS);
    if (result == WAIT_TIMEOUT) {
      LOG_ERROR_MESSAGE(L"Wait timed out");
    } else if (result == WAIT_FAILED) {
      LOG_ERROR_MESSAGE((L"Wait failed: %#X"), GetLastError());
    } else if (result == WAIT_ABANDONED) {
      LOG_ERROR_MESSAGE((L"Wait abandoned"));
    } else {
      LOG_ERROR_MESSAGE((L"Wait succeeded!"));
    }
  } else {
    LOG_ERROR_MESSAGE((L"Failed to create event with name '%s'"),
                      eventName.c_str());
  }
}

mozilla::Maybe<ToastArgs> NotificationCallback::ParseToastArguments(
    LPCWSTR invokedArgs) {
  ToastArgs parsedArgs;
  std::wistringstream args(invokedArgs);
  bool serverDisabled = true;

  for (std::wstring key, value;
       std::getline(args, key) && std::getline(args, value);) {
    if (key == kLaunchArgProgram) {
      serverDisabled = false;
    } else if (key == kLaunchArgProfile) {
      parsedArgs.profile = value;
    } else if (key == kLaunchArgTag) {
      parsedArgs.windowsTag = value;
    } else if (key == kLaunchArgAction) {
      // Remainder of args are from the Web Notification action, don't parse.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1781929.
      break;
    }
  }

  if (serverDisabled) {
    return mozilla::Nothing();
  }

  return mozilla::Some(parsedArgs);
}

std::tuple<path, mozilla::UniquePtr<wchar_t[]>>
NotificationCallback::BuildRunCommand(const ToastArgs& args) {
  path programPath = installDir / L"" MOZ_APP_NAME;
  programPath += L".exe";

  std::vector<const wchar_t*> childArgv;
  childArgv.push_back(programPath.c_str());

  if (!args.profile.empty()) {
    childArgv.push_back(L"--profile");
    childArgv.push_back(args.profile.c_str());
  } else {
    LOG_ERROR_MESSAGE(L"No profile; invocation will choose default profile");
  }

  if (!args.windowsTag.empty()) {
    childArgv.push_back(L"--notification-windowsTag");
    childArgv.push_back(args.windowsTag.c_str());
  } else {
    LOG_ERROR_MESSAGE(L"No windowsTag; invoking anyway");
  }

  return {programPath,
          mozilla::MakeCommandLine(childArgv.size(), childArgv.data())};
}
