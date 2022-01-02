// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_JOB_H_
#define SANDBOX_SRC_JOB_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/restricted_token_utils.h"

namespace sandbox {

// Handles the creation of job objects based on a security profile.
// Sample usage:
//   Job job;
//   job.Init(JOB_LOCKDOWN, nullptr);  //no job name
//   job.AssignProcessToJob(process_handle);
class Job {
 public:
  Job();

  ~Job();

  // Initializes and creates the job object. The security of the job is based
  // on the security_level parameter.
  // job_name can be nullptr if the job is unnamed.
  // If the chosen profile has too many ui restrictions, you can disable some
  // by specifying them in the ui_exceptions parameters.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD Init(JobLevel security_level,
             const wchar_t* job_name,
             DWORD ui_exceptions,
             size_t memory_limit);

  // Assigns the process referenced by process_handle to the job.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AssignProcessToJob(HANDLE process_handle);

  // Grants access to "handle" to the job. All processes in the job can
  // subsequently recognize and use the handle.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD UserHandleGrantAccess(HANDLE handle);

  // Revokes ownership to the job handle and returns it.
  // If the object is not yet initialized, it returns an invalid handle.
  base::win::ScopedHandle Take();

 private:
  // Handle to the job referenced by the object.
  base::win::ScopedHandle job_handle_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_JOB_H_
