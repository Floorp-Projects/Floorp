// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the job object.

#include "sandbox/win/src/job.h"

#include "base/win/scoped_process_information.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Tests the creation and destruction of the job.
TEST(JobTest, TestCreation) {
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0, 0));

    // check if the job exists.
    HANDLE job_handle =
        ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
    ASSERT_TRUE(job_handle);

    if (job_handle)
      CloseHandle(job_handle);
  }

  // Check if the job is destroyed when the object goes out of scope.
  HANDLE job_handle = ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
  ASSERT_TRUE(!job_handle);
  ASSERT_EQ(static_cast<DWORD>(ERROR_FILE_NOT_FOUND), ::GetLastError());
}

// Tests the method "Take".
TEST(JobTest, Take) {
  base::win::ScopedHandle job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0, 0));

    job_handle = job.Take();
    ASSERT_TRUE(job_handle.IsValid());
  }

  // Check to be sure that the job is still alive even after the object is gone
  // out of scope.
  HANDLE job_handle_dup =
      ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
  ASSERT_TRUE(job_handle_dup);

  // Remove all references.
  if (job_handle_dup)
    ::CloseHandle(job_handle_dup);

  job_handle.Close();

  // Check if the jbo is really dead.
  job_handle_dup = ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
  ASSERT_TRUE(!job_handle_dup);
  ASSERT_EQ(static_cast<DWORD>(ERROR_FILE_NOT_FOUND), ::GetLastError());
}

// Tests the ui exceptions
TEST(JobTest, TestExceptions) {
  base::win::ScopedHandle job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JOB_LOCKDOWN, L"my_test_job_name",
                       JOB_OBJECT_UILIMIT_READCLIPBOARD, 0));

    job_handle = job.Take();
    ASSERT_TRUE(job_handle.IsValid());

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    ASSERT_TRUE(::QueryInformationJobObject(
        job_handle.Get(), JobObjectBasicUIRestrictions, &jbur, size, &size));

    ASSERT_EQ(0u, jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD);
    job_handle.Close();
  }

  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0, 0));

    job_handle = job.Take();
    ASSERT_TRUE(job_handle.IsValid());

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    ASSERT_TRUE(::QueryInformationJobObject(
        job_handle.Get(), JobObjectBasicUIRestrictions, &jbur, size, &size));

    ASSERT_EQ(static_cast<DWORD>(JOB_OBJECT_UILIMIT_READCLIPBOARD),
              jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD);
  }
}

// Tests the error case when the job is initialized twice.
TEST(JobTest, DoubleInit) {
  // Create the job.
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.Init(JOB_LOCKDOWN, L"my_test_job_name", 0, 0));
  ASSERT_EQ(static_cast<DWORD>(ERROR_ALREADY_INITIALIZED),
            job.Init(JOB_LOCKDOWN, L"test", 0, 0));
}

// Tests the error case when we use a method and the object is not yet
// initialized.
TEST(JobTest, NoInit) {
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_NO_DATA),
            job.UserHandleGrantAccess(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_NO_DATA), job.AssignProcessToJob(nullptr));
  ASSERT_FALSE(job.Take().IsValid());
}

// Tests the initialization of the job with different security level.
TEST(JobTest, SecurityLevel) {
  Job job1;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job1.Init(JOB_LOCKDOWN, L"job1", 0, 0));

  Job job2;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job2.Init(JOB_RESTRICTED, L"job2", 0, 0));

  Job job3;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job3.Init(JOB_LIMITED_USER, L"job3", 0, 0));

  Job job4;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job4.Init(JOB_INTERACTIVE, L"job4", 0, 0));

  Job job5;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job5.Init(JOB_UNPROTECTED, L"job5", 0, 0));

  // JOB_NONE means we run without a job object so Init should fail.
  Job job6;
  ASSERT_EQ(static_cast<DWORD>(ERROR_BAD_ARGUMENTS),
            job6.Init(JOB_NONE, L"job6", 0, 0));

  Job job7;
  ASSERT_EQ(static_cast<DWORD>(ERROR_BAD_ARGUMENTS),
            job7.Init(static_cast<JobLevel>(JOB_NONE + 1), L"job7", 0, 0));
}

// Tests the method "AssignProcessToJob".
TEST(JobTest, ProcessInJob) {
  // Create the job.
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.Init(JOB_UNPROTECTED, L"job_test_process", 0, 0));

  wchar_t notepad[] = L"notepad";
  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION temp_process_info = {};
  ASSERT_TRUE(::CreateProcess(nullptr, notepad, nullptr, nullptr, false, 0,
                              nullptr, nullptr, &si, &temp_process_info));
  base::win::ScopedProcessInformation pi(temp_process_info);
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.AssignProcessToJob(pi.process_handle()));

  // Get the job handle.
  base::win::ScopedHandle job_handle = job.Take();

  // Check if the process is in the job.
  JOBOBJECT_BASIC_PROCESS_ID_LIST jbpidl = {0};
  DWORD size = sizeof(jbpidl);
  EXPECT_TRUE(::QueryInformationJobObject(
      job_handle.Get(), JobObjectBasicProcessIdList, &jbpidl, size, &size));

  EXPECT_EQ(1u, jbpidl.NumberOfAssignedProcesses);
  EXPECT_EQ(1u, jbpidl.NumberOfProcessIdsInList);
  EXPECT_EQ(pi.process_id(), jbpidl.ProcessIdList[0]);

  EXPECT_TRUE(::TerminateProcess(pi.process_handle(), 0));
}

}  // namespace sandbox
