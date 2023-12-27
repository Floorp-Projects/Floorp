/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinTaskScheduler.h"

#include <windows.h>
#include <comdef.h>
#include <sddl.h>
#include <securitybaseapi.h>
#include <taskschd.h>

#include "nsString.h"

#include "mozilla/RefPtr.h"
#include "mozilla/ResultVariant.h"

using namespace mozilla;

struct SysFreeStringDeleter {
  void operator()(BSTR aPtr) { ::SysFreeString(aPtr); }
};
using BStrPtr = mozilla::UniquePtr<OLECHAR, SysFreeStringDeleter>;

static nsresult ToNotFoundOrFailure(HRESULT hr) {
  if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    return NS_ERROR_FILE_NOT_FOUND;
  } else {
    return NS_ERROR_FAILURE;
  }
}

static nsresult ToAlreadyExistsOrFailure(HRESULT hr) {
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
    return NS_ERROR_FILE_ALREADY_EXISTS;
  } else {
    return NS_ERROR_FAILURE;
  }
}

[[nodiscard]] static Result<RefPtr<ITaskFolder>, HRESULT> GetTaskFolder(
    const char16_t* aFolderName) {
  HRESULT hr;
  RefPtr<ITaskService> scheduler = nullptr;

  hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                        IID_ITaskService, getter_AddRefs(scheduler));
  if (FAILED(hr)) {
    return Err(hr);
  }

  // Connect to the local Task Scheduler.
  hr = scheduler->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{});
  if (FAILED(hr)) {
    return Err(hr);
  }

  BStrPtr bstrFolderName =
      BStrPtr(::SysAllocString(reinterpret_cast<const OLECHAR*>(aFolderName)));

  RefPtr<ITaskFolder> folder = nullptr;
  hr = scheduler->GetFolder(bstrFolderName.get(), getter_AddRefs(folder));
  if (FAILED(hr)) {
    return Err(hr);
  }

  return folder;
}

[[nodiscard]] static Result<RefPtr<IRegisteredTask>, HRESULT> GetRegisteredTask(
    const char16_t* aFolderName, const char16_t* aTaskName) {
  auto folder = GetTaskFolder(aFolderName);
  if (!folder.isOk()) {
    return Err(folder.unwrapErr());
  }

  BStrPtr bstrTaskName =
      BStrPtr(::SysAllocString(reinterpret_cast<const OLECHAR*>(aTaskName)));

  RefPtr<IRegisteredTask> task = nullptr;
  HRESULT hr =
      folder.unwrap()->GetTask(bstrTaskName.get(), getter_AddRefs(task));
  if (FAILED(hr)) {
    return Err(hr);
  }

  return task;
}

NS_IMPL_ISUPPORTS(nsWinTaskSchedulerService, nsIWinTaskSchedulerService)

NS_IMETHODIMP
nsWinTaskSchedulerService::GetTaskXML(const char16_t* aFolderName,
                                      const char16_t* aTaskName,
                                      nsAString& aResult) {
  if (!aFolderName || !aTaskName) {
    return NS_ERROR_NULL_POINTER;
  }

  auto task = GetRegisteredTask(aFolderName, aTaskName);
  if (!task.isOk()) {
    return ToNotFoundOrFailure(task.unwrapErr());
  }

  {
    BSTR bstrXml = nullptr;
    if (FAILED(task.unwrap()->get_Xml(&bstrXml))) {
      return NS_ERROR_FAILURE;
    }

    aResult.Assign(bstrXml, ::SysStringLen(bstrXml));
    ::SysFreeString(bstrXml);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::GetCurrentUserSid(nsAString& aUserSid) {
#ifndef XP_WIN
  return NS_ERROR_NOT_IMPLEMENTED;
#else  // !XP_WIN
  DWORD tokenLen;
  LPWSTR stringSid;
  BYTE tokenBuf[TOKEN_USER_MAX_SIZE];
  PTOKEN_USER tokenInfo = reinterpret_cast<PTOKEN_USER>(tokenBuf);
  BOOL success = GetTokenInformation(GetCurrentProcessToken(), TokenUser,
                                     tokenInfo, sizeof(tokenBuf), &tokenLen);
  if (!success) {
    return NS_ERROR_FAILURE;
  }
  success = ConvertSidToStringSidW(tokenInfo->User.Sid, &stringSid);
  if (!success) {
    return NS_ERROR_ABORT;
  }
  aUserSid.Assign(stringSid);
  LocalFree(stringSid);
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsWinTaskSchedulerService::RegisterTask(const char16_t* aFolderName,
                                        const char16_t* aTaskName,
                                        const char16_t* aDefinitionXML,
                                        bool aUpdateExisting) {
  if (!aFolderName || !aTaskName || !aDefinitionXML) {
    return NS_ERROR_NULL_POINTER;
  }

  auto folder = GetTaskFolder(aFolderName);
  if (!folder.isOk()) {
    return ToNotFoundOrFailure(folder.unwrapErr());
  }

  BStrPtr bstrTaskName =
      BStrPtr(::SysAllocString(reinterpret_cast<const OLECHAR*>(aTaskName)));
  BStrPtr bstrXml = BStrPtr(
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(aDefinitionXML)));
  LONG flags = aUpdateExisting ? TASK_CREATE_OR_UPDATE : TASK_CREATE;
  TASK_LOGON_TYPE logonType = TASK_LOGON_INTERACTIVE_TOKEN;

  // The outparam is not needed, but not documented as optional.
  RefPtr<IRegisteredTask> unusedTaskOutput = nullptr;
  HRESULT hr = folder.unwrap()->RegisterTask(
      bstrTaskName.get(), bstrXml.get(), flags, VARIANT{} /* userId */,
      VARIANT{} /* password */, logonType, VARIANT{} /* sddl */,
      getter_AddRefs(unusedTaskOutput));

  if (FAILED(hr)) {
    return ToAlreadyExistsOrFailure(hr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::ValidateTaskDefinition(
    const char16_t* aDefinitionXML, int32_t* aResult) {
  if (!aDefinitionXML) {
    return NS_ERROR_NULL_POINTER;
  }

  auto folder = GetTaskFolder(reinterpret_cast<const char16_t*>(L"\\"));
  if (!folder.isOk()) {
    return NS_ERROR_FAILURE;
  }

  BStrPtr bstrXml = BStrPtr(
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(aDefinitionXML)));
  LONG flags = TASK_VALIDATE_ONLY;
  TASK_LOGON_TYPE logonType = TASK_LOGON_INTERACTIVE_TOKEN;

  // The outparam is not needed, but not documented as optional.
  RefPtr<IRegisteredTask> unusedTaskOutput = nullptr;
  HRESULT hr = folder.unwrap()->RegisterTask(
      nullptr /* path */, bstrXml.get(), flags, VARIANT{} /* userId */,
      VARIANT{} /* password */, logonType, VARIANT{} /* sddl */,
      getter_AddRefs(unusedTaskOutput));

  if (aResult) {
    *aResult = hr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::DeleteTask(const char16_t* aFolderName,
                                      const char16_t* aTaskName) {
  if (!aFolderName || !aTaskName) {
    return NS_ERROR_NULL_POINTER;
  }

  auto folder = GetTaskFolder(aFolderName);
  if (!folder.isOk()) {
    return ToNotFoundOrFailure(folder.unwrapErr());
  }

  BStrPtr bstrTaskName =
      BStrPtr(::SysAllocString(reinterpret_cast<const OLECHAR*>(aTaskName)));

  HRESULT hr = folder.unwrap()->DeleteTask(bstrTaskName.get(), 0 /* flags */);
  if (FAILED(hr)) {
    return ToNotFoundOrFailure(hr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::GetFolderTasks(const char16_t* aFolderName,
                                          nsTArray<nsString>& aResult) {
  if (!aFolderName) {
    return NS_ERROR_NULL_POINTER;
  }

  auto folder = GetTaskFolder(aFolderName);
  if (!folder.isOk()) {
    return ToNotFoundOrFailure(folder.unwrapErr());
  }

  RefPtr<IRegisteredTaskCollection> taskCollection = nullptr;
  if (FAILED(folder.unwrap()->GetTasks(TASK_ENUM_HIDDEN,
                                       getter_AddRefs(taskCollection)))) {
    return NS_ERROR_FAILURE;
  }

  LONG taskCount = 0;
  if (FAILED(taskCollection->get_Count(&taskCount))) {
    return NS_ERROR_FAILURE;
  }

  aResult.Clear();

  for (LONG i = 0; i < taskCount; ++i) {
    RefPtr<IRegisteredTask> task = nullptr;

    // nb: Collections are indexed from 1.
    if (FAILED(taskCollection->get_Item(_variant_t(i + 1),
                                        getter_AddRefs(task)))) {
      return NS_ERROR_FAILURE;
    }

    BStrPtr bstrTaskName;
    {
      BSTR tempTaskName = nullptr;
      if (FAILED(task->get_Name(&tempTaskName))) {
        return NS_ERROR_FAILURE;
      }
      bstrTaskName = BStrPtr(tempTaskName);
    }

    aResult.AppendElement(nsString(bstrTaskName.get()));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::CreateFolder(const char16_t* aParentFolderName,
                                        const char16_t* aSubFolderName) {
  if (!aParentFolderName || !aSubFolderName) {
    return NS_ERROR_NULL_POINTER;
  }

  auto parentFolder = GetTaskFolder(aParentFolderName);
  if (!parentFolder.isOk()) {
    return ToNotFoundOrFailure(parentFolder.unwrapErr());
  }

  BStrPtr bstrSubFolderName = BStrPtr(
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(aSubFolderName)));

  HRESULT hr = parentFolder.unwrap()->CreateFolder(bstrSubFolderName.get(),
                                                   VARIANT{},  // sddl
                                                   nullptr);   // ppFolder

  if (FAILED(hr)) {
    return ToAlreadyExistsOrFailure(hr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWinTaskSchedulerService::DeleteFolder(const char16_t* aParentFolderName,
                                        const char16_t* aSubFolderName) {
  if (!aParentFolderName || !aSubFolderName) {
    return NS_ERROR_NULL_POINTER;
  }

  auto parentFolder = GetTaskFolder(aParentFolderName);
  if (!parentFolder.isOk()) {
    return ToNotFoundOrFailure(parentFolder.unwrapErr());
  }

  BStrPtr bstrSubFolderName = BStrPtr(
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(aSubFolderName)));

  HRESULT hr = parentFolder.unwrap()->DeleteFolder(bstrSubFolderName.get(),
                                                   0 /* flags */);

  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_DIR_NOT_EMPTY)) {
      return NS_ERROR_FILE_DIR_NOT_EMPTY;
    } else {
      return ToNotFoundOrFailure(hr);
    }
  }

  return NS_OK;
}
