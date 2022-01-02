/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_H__
#define __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_H__

#include <windows.h>

// uniqueToken should be a string unique to the installation, so that a
// separate task can be created for each installation. Typically this will be
// the install hash string.
HRESULT RegisterTask(const wchar_t* uniqueToken, BSTR startTime = nullptr);
HRESULT UpdateTask(const wchar_t* uniqueToken);

enum class WhichTasks {
  WdbaTaskOnly,
  AllTasksForInstallation,
};
HRESULT RemoveTasks(const wchar_t* uniqueToken, WhichTasks tasksToRemove);

#endif  // __DEFAULT_BROWSER_AGENT_SCHEDULED_TASK_H__
