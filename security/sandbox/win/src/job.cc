// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/job.h"

#include "base/win/windows_version.h"
#include "sandbox/win/src/restricted_token.h"

namespace sandbox {

Job::~Job() {
  if (job_handle_)
    ::CloseHandle(job_handle_);
};

DWORD Job::Init(JobLevel security_level,
                const wchar_t* job_name,
                DWORD ui_exceptions,
                size_t memory_limit) {
  if (job_handle_)
    return ERROR_ALREADY_INITIALIZED;

  job_handle_ = ::CreateJobObject(NULL,   // No security attribute
                                  job_name);
  if (!job_handle_)
    return ::GetLastError();

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
  JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};

  // Set the settings for the different security levels. Note: The higher levels
  // inherit from the lower levels.
  switch (security_level) {
    case JOB_LOCKDOWN: {
      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    }
    case JOB_RESTRICTED: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_READCLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_HANDLES;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_GLOBALATOMS;
    }
    case JOB_LIMITED_USER: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
      jeli.BasicLimitInformation.ActiveProcessLimit = 1;
    }
    case JOB_INTERACTIVE: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DESKTOP;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_EXITWINDOWS;
    }
    case JOB_UNPROTECTED: {
      if (memory_limit) {
        jeli.BasicLimitInformation.LimitFlags |=
            JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        jeli.ProcessMemoryLimit = memory_limit;
      }

      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      break;
    }
    default: {
      return ERROR_BAD_ARGUMENTS;
    }
  }

  if (FALSE == ::SetInformationJobObject(job_handle_,
                                         JobObjectExtendedLimitInformation,
                                         &jeli,
                                         sizeof(jeli))) {
    return ::GetLastError();
  }

  jbur.UIRestrictionsClass = jbur.UIRestrictionsClass & (~ui_exceptions);
  if (FALSE == ::SetInformationJobObject(job_handle_,
                                         JobObjectBasicUIRestrictions,
                                         &jbur,
                                         sizeof(jbur))) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD Job::UserHandleGrantAccess(HANDLE handle) {
  if (!job_handle_)
    return ERROR_NO_DATA;

  if (!::UserHandleGrantAccess(handle,
                               job_handle_,
                               TRUE)) {  // Access allowed.
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

HANDLE Job::Detach() {
  HANDLE handle_temp = job_handle_;
  job_handle_ = NULL;
  return handle_temp;
}

DWORD Job::AssignProcessToJob(HANDLE process_handle) {
  if (!job_handle_)
    return ERROR_NO_DATA;

  if (FALSE == ::AssignProcessToJobObject(job_handle_, process_handle))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

}  // namespace sandbox
