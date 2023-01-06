/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ToastNotificationHeaderOnlyUtils_h
#define mozilla_ToastNotificationHeaderOnlyUtils_h

/**
 * This header is intended for self-contained, header-only, utility code to
 * share between Windows toast notification code in firefox.exe and
 * notificationserver.dll.
 */

namespace mozilla::widget::toastnotification {

const wchar_t kLaunchArgProgram[] = L"program";
const wchar_t kLaunchArgProfile[] = L"profile";
const wchar_t kLaunchArgUrl[] = L"launchUrl";
const wchar_t kLaunchArgPrivilegedName[] = L"privilegedName";
const wchar_t kLaunchArgTag[] = L"windowsTag";
const wchar_t kLaunchArgAction[] = L"action";

}  // namespace mozilla::widget::toastnotification

#endif  // mozilla_ToastNotificationHeaderOnlyUtils_h
