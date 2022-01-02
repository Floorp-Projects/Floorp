// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/job.h"

#include <stddef.h>
#include <utility>

#include "base/win/windows_version.h"
#include "sandbox/win/src/restricted_token.h"

namespace sandbox {

Job::Job() : job_handle_(nullptr) {}

Job::~Job() {}

DWORD Job::Init(JobLevel security_level,
                const wchar_t* job_name,
                DWORD ui_exceptions,
                size_t memory_limit) {
  if (job_handle_.IsValid())
    return ERROR_ALREADY_INITIALIZED;

  job_handle_.Set(::CreateJobObject(nullptr,  // No security attribute
                                    job_name));
  if (!job_handle_.IsValid())
    return ::GetLastError();

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
  JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {};

  // Set the settings for the different security levels. Note: The higher levels
  // inherit from the lower levels.
  switch (security_level) {
    case JOB_LOCKDOWN: {
      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
      FALLTHROUGH;
    }
    case JOB_RESTRICTED: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_READCLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_HANDLES;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_GLOBALATOMS;
      FALLTHROUGH;
    }
    case JOB_LIMITED_USER: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
      jeli.BasicLimitInformation.ActiveProcessLimit = 1;
      FALLTHROUGH;
    }
    case JOB_INTERACTIVE: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DESKTOP;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_EXITWINDOWS;
      FALLTHROUGH;
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
    default: { return ERROR_BAD_ARGUMENTS; }
  }

  if (!::SetInformationJobObject(job_handle_.Get(),
                                 JobObjectExtendedLimitInformation, &jeli,
                                 sizeof(jeli))) {
    return ::GetLastError();
  }

  jbur.UIRestrictionsClass = jbur.UIRestrictionsClass & (~ui_exceptions);
  if (!::SetInformationJobObject(job_handle_.Get(),
                                 JobObjectBasicUIRestrictions, &jbur,
                                 sizeof(jbur))) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD Job::UserHandleGrantAccess(HANDLE handle) {
  if (!job_handle_.IsValid())
    return ERROR_NO_DATA;

  if (!::UserHandleGrantAccess(handle, job_handle_.Get(),
                               true)) {  // Access allowed.
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

base::win::ScopedHandle Job::Take() {
  return std::move(job_handle_);
}

DWORD Job::AssignProcessToJob(HANDLE process_handle) {
  if (!job_handle_.IsValid())
    return ERROR_NO_DATA;

  if (!::AssignProcessToJobObject(job_handle_.Get(), process_handle))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

}  // namespace sandbox
