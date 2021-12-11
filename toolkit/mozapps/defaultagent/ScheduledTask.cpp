/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScheduledTask.h"

#include <string>
#include <time.h>

#include <comutil.h>
#include <taskschd.h>

#include "readstrings.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "WindowsDefaultBrowser.h"

#include "DefaultBrowser.h"

const wchar_t* kTaskVendor = L"" MOZ_APP_VENDOR;
// kTaskName should have the unique token appended before being used.
const wchar_t* kTaskName = L"" MOZ_APP_DISPLAYNAME " Default Browser Agent ";

// The task scheduler requires its time values to come in the form of a string
// in the format YYYY-MM-DDTHH:MM:SSZ. This format string is used to get that
// out of the C library wcsftime function.
const wchar_t* kTimeFormat = L"%Y-%m-%dT%H:%M:%SZ";
// The expanded time string should always be this length, for example:
// 2020-02-12T16:59:32Z
const size_t kTimeStrMaxLen = 20;

#define ENSURE(x)         \
  if (FAILED(hr = (x))) { \
    LOG_ERROR(hr);        \
    return hr;            \
  }

struct SysFreeStringDeleter {
  void operator()(BSTR aPtr) { ::SysFreeString(aPtr); }
};
using BStrPtr = mozilla::UniquePtr<OLECHAR, SysFreeStringDeleter>;

bool GetTaskDescription(mozilla::UniquePtr<wchar_t[]>& description) {
  mozilla::UniquePtr<wchar_t[]> installPath;
  bool success = GetInstallDirectory(installPath);
  if (!success) {
    LOG_ERROR_MESSAGE(L"Failed to get install directory");
    return false;
  }
  const wchar_t* iniFormat = L"%s\\defaultagent_localized.ini";
  int bufferSize = _scwprintf(iniFormat, installPath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> iniPath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(iniPath.get(), bufferSize, _TRUNCATE, iniFormat,
               installPath.get());

  IniReader reader(iniPath.get());
  reader.AddKey("DefaultBrowserAgentTaskDescription", &description);
  int status = reader.Read();
  if (status != OK) {
    LOG_ERROR_MESSAGE(L"Failed to read task description: %d", status);
    return false;
  }
  return true;
}

HRESULT RegisterTask(const wchar_t* uniqueToken,
                     BSTR startTime /* = nullptr */) {
  // Do data migration during the task installation. This might seem like it
  // belongs in UpdateTask, but we want to be able to call
  //    RemoveTasks();
  //    RegisterTask();
  // and still have data migration happen. Also, UpdateTask calls this function,
  // so migration will still get run in that case.
  MaybeMigrateCurrentDefault();

  // Make sure we don't try to register a task that already exists.
  RemoveTasks(uniqueToken, WhichTasks::WdbaTaskOnly);

  // If we create a folder and then fail to create the task, we need to
  // remember to delete the folder so that whatever set of permissions it ends
  // up with doesn't interfere with trying to create the task again later, and
  // so that we don't just leave an empty folder behind.
  bool createdFolder = false;

  HRESULT hr = S_OK;
  RefPtr<ITaskService> scheduler;
  ENSURE(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, getter_AddRefs(scheduler)));

  ENSURE(scheduler->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{}));

  RefPtr<ITaskFolder> rootFolder;
  BStrPtr rootFolderBStr = BStrPtr(SysAllocString(L"\\"));
  ENSURE(
      scheduler->GetFolder(rootFolderBStr.get(), getter_AddRefs(rootFolder)));

  RefPtr<ITaskFolder> taskFolder;
  BStrPtr vendorBStr = BStrPtr(SysAllocString(kTaskVendor));
  if (FAILED(rootFolder->GetFolder(vendorBStr.get(),
                                   getter_AddRefs(taskFolder)))) {
    hr = rootFolder->CreateFolder(vendorBStr.get(), VARIANT{},
                                  getter_AddRefs(taskFolder));
    if (SUCCEEDED(hr)) {
      createdFolder = true;
    } else if (hr != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
      // The folder already existing isn't an error, but anything else is.
      LOG_ERROR(hr);
      return hr;
    }
  }

  auto cleanupFolder =
      mozilla::MakeScopeExit([hr, createdFolder, &rootFolder, &vendorBStr] {
        if (createdFolder && FAILED(hr)) {
          // If this fails, we can't really handle that intelligently, so
          // don't even bother to check the return code.
          rootFolder->DeleteFolder(vendorBStr.get(), 0);
        }
      });

  RefPtr<ITaskDefinition> newTask;
  ENSURE(scheduler->NewTask(0, getter_AddRefs(newTask)));

  mozilla::UniquePtr<wchar_t[]> description;
  if (!GetTaskDescription(description)) {
    return E_FAIL;
  }
  BStrPtr descriptionBstr = BStrPtr(SysAllocString(description.get()));

  RefPtr<IRegistrationInfo> taskRegistration;
  ENSURE(newTask->get_RegistrationInfo(getter_AddRefs(taskRegistration)));
  ENSURE(taskRegistration->put_Description(descriptionBstr.get()));

  RefPtr<ITaskSettings> taskSettings;
  ENSURE(newTask->get_Settings(getter_AddRefs(taskSettings)));
  ENSURE(taskSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE));
  ENSURE(taskSettings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW));
  ENSURE(taskSettings->put_StartWhenAvailable(VARIANT_TRUE));
  ENSURE(taskSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
  // This cryptic string means "35 minutes". So, if the task runs for longer
  // than that, the process will be killed, because that should never happen.
  BStrPtr execTimeLimitBStr = BStrPtr(SysAllocString(L"PT35M"));
  ENSURE(taskSettings->put_ExecutionTimeLimit(execTimeLimitBStr.get()));

  RefPtr<IRegistrationInfo> regInfo;
  ENSURE(newTask->get_RegistrationInfo(getter_AddRefs(regInfo)));

  ENSURE(regInfo->put_Author(vendorBStr.get()));

  RefPtr<ITriggerCollection> triggers;
  ENSURE(newTask->get_Triggers(getter_AddRefs(triggers)));

  RefPtr<ITrigger> newTrigger;
  ENSURE(triggers->Create(TASK_TRIGGER_DAILY, getter_AddRefs(newTrigger)));

  RefPtr<IDailyTrigger> dailyTrigger;
  ENSURE(newTrigger->QueryInterface(IID_IDailyTrigger,
                                    getter_AddRefs(dailyTrigger)));

  if (startTime) {
    ENSURE(dailyTrigger->put_StartBoundary(startTime));
  } else {
    // The time that the task is scheduled to run at every day is taken from the
    // time in the trigger's StartBoundary property. We'll set this to the
    // current time, on the theory that the time at which we're being installed
    // is a time that the computer is likely to be on other days. If our
    // theory is wrong and the computer is offline at the scheduled time, then
    // because we've set StartWhenAvailable above, the task will run whenever
    // it wakes up. Since our task is entirely in the background and doesn't use
    // a lot of resources, we're not concerned about it bothering the user if it
    // runs while they're actively using this computer.
    time_t now_t = time(nullptr);
    // Subtract a minute from the current time, to avoid "winning" a potential
    // race with the scheduler that might have it start the task immediately
    // after we register it, if we finish doing that and then it evaluates the
    // trigger during the same second. We haven't seen this happen in practice,
    // but there's no documented guarantee that it won't, so let's be sure.
    now_t -= 60;

    tm now_tm;
    errno_t errno_rv = gmtime_s(&now_tm, &now_t);
    if (errno_rv != 0) {
      // The C runtime has a (private) function to convert Win32 error codes to
      // errno values, but there's nothing that goes the other way, and it
      // isn't worth including one here for something that's this unlikely to
      // fail anyway. So just return a generic error.
      hr = HRESULT_FROM_WIN32(ERROR_INVALID_TIME);
      LOG_ERROR(hr);
      return hr;
    }

    mozilla::UniquePtr<wchar_t[]> timeStr =
        mozilla::MakeUnique<wchar_t[]>(kTimeStrMaxLen + 1);

    if (wcsftime(timeStr.get(), kTimeStrMaxLen + 1, kTimeFormat, &now_tm) ==
        0) {
      hr = E_NOT_SUFFICIENT_BUFFER;
      LOG_ERROR(hr);
      return hr;
    }

    BStrPtr startTimeBStr = BStrPtr(SysAllocString(timeStr.get()));
    ENSURE(dailyTrigger->put_StartBoundary(startTimeBStr.get()));
  }

  ENSURE(dailyTrigger->put_DaysInterval(1));

  RefPtr<IActionCollection> actions;
  ENSURE(newTask->get_Actions(getter_AddRefs(actions)));

  RefPtr<IAction> action;
  ENSURE(actions->Create(TASK_ACTION_EXEC, getter_AddRefs(action)));

  RefPtr<IExecAction> execAction;
  ENSURE(action->QueryInterface(IID_IExecAction, getter_AddRefs(execAction)));

  BStrPtr binaryPathBStr =
      BStrPtr(SysAllocString(mozilla::GetFullBinaryPath().get()));
  ENSURE(execAction->put_Path(binaryPathBStr.get()));

  std::wstring taskArgs = L"do-task \"";
  taskArgs += uniqueToken;
  taskArgs += L"\"";
  BStrPtr argsBStr = BStrPtr(SysAllocString(taskArgs.c_str()));
  ENSURE(execAction->put_Arguments(argsBStr.get()));

  std::wstring taskName(kTaskName);
  taskName += uniqueToken;
  BStrPtr taskNameBStr = BStrPtr(SysAllocString(taskName.c_str()));

  RefPtr<IRegisteredTask> registeredTask;
  ENSURE(taskFolder->RegisterTaskDefinition(
      taskNameBStr.get(), newTask, TASK_CREATE_OR_UPDATE, VARIANT{}, VARIANT{},
      TASK_LOGON_INTERACTIVE_TOKEN, VARIANT{}, getter_AddRefs(registeredTask)));

  return hr;
}

HRESULT UpdateTask(const wchar_t* uniqueToken) {
  RefPtr<ITaskService> scheduler;
  HRESULT hr = S_OK;
  ENSURE(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, getter_AddRefs(scheduler)));

  ENSURE(scheduler->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{}));

  RefPtr<ITaskFolder> taskFolder;
  BStrPtr folderBStr = BStrPtr(SysAllocString(kTaskVendor));

  if (FAILED(
          scheduler->GetFolder(folderBStr.get(), getter_AddRefs(taskFolder)))) {
    // If our folder doesn't exist, create it and the task.
    return RegisterTask(uniqueToken);
  }

  std::wstring taskName(kTaskName);
  taskName += uniqueToken;
  BStrPtr taskNameBStr = BStrPtr(SysAllocString(taskName.c_str()));

  RefPtr<IRegisteredTask> task;
  if (FAILED(taskFolder->GetTask(taskNameBStr.get(), getter_AddRefs(task)))) {
    // If our task doesn't exist at all, just create one.
    return RegisterTask(uniqueToken);
  }

  // If we have a task registered already, we need to recreate it because
  // something might have changed that we need to update. But we don't
  // want to restart the schedule from now, because that might mean the
  // task never runs at all for e.g. Nightly. So create a new task, but
  // first get and preserve the existing trigger.
  RefPtr<ITaskDefinition> definition;
  if (FAILED(task->get_Definition(getter_AddRefs(definition)))) {
    // This task is broken, make a new one.
    return RegisterTask(uniqueToken);
  }

  RefPtr<ITriggerCollection> triggerList;
  if (FAILED(definition->get_Triggers(getter_AddRefs(triggerList)))) {
    // This task is broken, make a new one.
    return RegisterTask(uniqueToken);
  }

  RefPtr<ITrigger> trigger;
  if (FAILED(triggerList->get_Item(1, getter_AddRefs(trigger)))) {
    // This task is broken, make a new one.
    return RegisterTask(uniqueToken);
  }

  BSTR startTimeBstr;
  if (FAILED(trigger->get_StartBoundary(&startTimeBstr))) {
    // This task is broken, make a new one.
    return RegisterTask(uniqueToken);
  }
  BStrPtr startTime(startTimeBstr);

  return RegisterTask(uniqueToken, startTime.get());
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
