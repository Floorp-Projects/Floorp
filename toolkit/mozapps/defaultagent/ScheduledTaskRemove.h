/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_REMOVE_H__
#define __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_REMOVE_H__

#include <windows.h>
#include <wtypes.h>

#include <oleauto.h>

#include "mozilla/UniquePtr.h"

namespace mozilla::default_agent {

struct SysFreeStringDeleter {
  void operator()(BSTR aPtr) { ::SysFreeString(aPtr); }
};
using BStrPtr = mozilla::UniquePtr<OLECHAR, SysFreeStringDeleter>;

static const wchar_t* kTaskVendor = L"" MOZ_APP_VENDOR;
// kTaskName should have the unique token appended before being used.
static const wchar_t* kTaskName =
    L"" MOZ_APP_DISPLAYNAME " Default Browser Agent ";

enum class WhichTasks {
  WdbaTaskOnly,
  AllTasksForInstallation,
};
HRESULT RemoveTasks(const wchar_t* uniqueToken, WhichTasks tasksToRemove);

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_REMOVE_H__
