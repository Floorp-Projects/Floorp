/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScheduledTaskRemove.h"

#include <string>

#include <comutil.h>
#include <taskschd.h>

#include "EventLog.h"
#include "mozilla/RefPtr.h"

namespace mozilla::default_agent {

#define ENSURE(x)         \
  if (FAILED(hr = (x))) { \
    LOG_ERROR(hr);        \
    return hr;            \
  }

bool EndsWith(const wchar_t* string, const wchar_t* suffix) {
  size_t string_len = wcslen(string);
  size_t suffix_len = wcslen(suffix);
  if (suffix_len > string_len) {
    return false;
  }
  const wchar_t* substring = string + string_len - suffix_len;
  return wcscmp(substring, suffix) == 0;
}

HRESULT RemoveTasks(const wchar_t* uniqueToken, WhichTasks tasksToRemove) {
  if (!uniqueToken || wcslen(uniqueToken) == 0) {
    return E_INVALIDARG;
  }

  RefPtr<ITaskService> scheduler;
  HRESULT hr = S_OK;
  ENSURE(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, getter_AddRefs(scheduler)));

  ENSURE(scheduler->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{}));

  RefPtr<ITaskFolder> taskFolder;
  BStrPtr folderBStr(SysAllocString(kTaskVendor));

  hr = scheduler->GetFolder(folderBStr.get(), getter_AddRefs(taskFolder));
  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      // Don't return an error code if our folder doesn't exist,
      // because that just means it's been removed already.
      return S_OK;
    } else {
      return hr;
    }
  }

  RefPtr<IRegisteredTaskCollection> tasksInFolder;
  ENSURE(taskFolder->GetTasks(TASK_ENUM_HIDDEN, getter_AddRefs(tasksInFolder)));

  LONG numTasks = 0;
  ENSURE(tasksInFolder->get_Count(&numTasks));

  std::wstring WdbaTaskName(kTaskName);
  WdbaTaskName += uniqueToken;

  // This will be set to the last error that we encounter while deleting tasks.
  // This allows us to keep attempting to remove the remaining tasks, even if
  // we encounter an error, while still preserving what error we encountered so
  // we can return it from this function.
  HRESULT deleteResult = S_OK;
  // Set to true if we intentionally skip any tasks.
  bool tasksSkipped = false;

  for (LONG i = 0; i < numTasks; ++i) {
    RefPtr<IRegisteredTask> task;
    // IRegisteredTaskCollection's are 1-indexed.
    hr = tasksInFolder->get_Item(_variant_t(i + 1), getter_AddRefs(task));
    if (FAILED(hr)) {
      deleteResult = hr;
      continue;
    }

    BSTR taskName;
    hr = task->get_Name(&taskName);
    if (FAILED(hr)) {
      deleteResult = hr;
      continue;
    }
    // Automatically free taskName when we are done with it.
    BStrPtr uniqueTaskName(taskName);

    if (tasksToRemove == WhichTasks::WdbaTaskOnly) {
      if (WdbaTaskName.compare(taskName) != 0) {
        tasksSkipped = true;
        continue;
      }
    } else {  // tasksToRemove == WhichTasks::AllTasksForInstallation
      if (!EndsWith(taskName, uniqueToken)) {
        tasksSkipped = true;
        continue;
      }
    }

    hr = taskFolder->DeleteTask(taskName, 0 /* flags */);
    if (FAILED(hr)) {
      deleteResult = hr;
    }
  }

  // If we successfully removed all the tasks, delete the folder too.
  if (!tasksSkipped && SUCCEEDED(deleteResult)) {
    RefPtr<ITaskFolder> rootFolder;
    BStrPtr rootFolderBStr = BStrPtr(SysAllocString(L"\\"));
    ENSURE(
        scheduler->GetFolder(rootFolderBStr.get(), getter_AddRefs(rootFolder)));
    ENSURE(rootFolder->DeleteFolder(folderBStr.get(), 0));
  }

  return deleteResult;
}

}  // namespace mozilla::default_agent
