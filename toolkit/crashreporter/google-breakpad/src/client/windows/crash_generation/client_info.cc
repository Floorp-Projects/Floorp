// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "client/windows/crash_generation/client_info.h"

namespace google_breakpad {

ClientInfo::ClientInfo(CrashGenerationServer* crash_server,
                       DWORD pid,
                       MINIDUMP_TYPE dump_type,
                       DWORD* thread_id,
                       EXCEPTION_POINTERS** ex_info,
                       MDRawAssertionInfo* assert_info)
    : crash_server_(crash_server),
      pid_(pid),
      dump_type_(dump_type),
      ex_info_(ex_info),
      assert_info_(assert_info),
      thread_id_(thread_id),
      process_handle_(NULL),
      dump_requested_handle_(NULL),
      dump_generated_handle_(NULL),
      dump_request_wait_handle_(NULL),
      process_exit_wait_handle_(NULL) {
}

bool ClientInfo::Initialize() {
  process_handle_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid_);
  if (!process_handle_) {
    return false;
  }

  dump_requested_handle_ = CreateEvent(NULL,    // Security attributes.
                                       TRUE,    // Manual reset.
                                       FALSE,   // Initial state.
                                       NULL);   // Name.
  if (!dump_requested_handle_) {
    return false;
  }

  dump_generated_handle_ = CreateEvent(NULL,    // Security attributes.
                                       TRUE,    // Manual reset.
                                       FALSE,   // Initial state.
                                       NULL);   // Name.
  return dump_generated_handle_ != NULL;
}

ClientInfo::~ClientInfo() {
  if (process_handle_) {
    CloseHandle(process_handle_);
  }

  if (dump_requested_handle_) {
    CloseHandle(dump_requested_handle_);
  }

  if (dump_generated_handle_) {
    CloseHandle(dump_generated_handle_);
  }

  if (dump_request_wait_handle_) {
    // Wait for callbacks that might already be running to finish.
    UnregisterWaitEx(dump_request_wait_handle_, INVALID_HANDLE_VALUE);
  }

  if (process_exit_wait_handle_) {
    // Wait for the callback that might already be running to finish.
    UnregisterWaitEx(process_exit_wait_handle_, INVALID_HANDLE_VALUE);
  }
}

bool ClientInfo::UnregisterWaits() {
  bool success = true;

  if (dump_request_wait_handle_) {
    if (!UnregisterWait(dump_request_wait_handle_)) {
      success = false;
    } else {
      dump_request_wait_handle_ = NULL;
    }
  }

  if (process_exit_wait_handle_) {
    if (!UnregisterWait(process_exit_wait_handle_)) {
      success = false;
    } else {
      process_exit_wait_handle_ = NULL;
    }
  }

  return success;
}

bool ClientInfo::GetClientExceptionInfo(
    EXCEPTION_POINTERS** ex_info) const {
  SIZE_T bytes_count = 0;
  if (!ReadProcessMemory(process_handle_,
                         ex_info_,
                         ex_info,
                         sizeof(*ex_info),
                         &bytes_count)) {
    return false;
  }

  return bytes_count == sizeof(*ex_info);
}

bool ClientInfo::GetClientThreadId(DWORD* thread_id) const {
  SIZE_T bytes_count = 0;
  if (!ReadProcessMemory(process_handle_,
                         thread_id_,
                         thread_id,
                         sizeof(*thread_id),
                         &bytes_count)) {
    return false;
  }

  return bytes_count == sizeof(*thread_id);
}

}  // namespace google_breakpad
