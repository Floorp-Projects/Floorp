// Copyright (c) 2006, Google Inc.
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

#include <ObjBase.h>

#include <cstdio>

#include "client/windows/handler/exception_handler.h"
#include "common/windows/guid_string.h"

namespace google_airbag {

ExceptionHandler *ExceptionHandler::current_handler_ = NULL;

ExceptionHandler::ExceptionHandler(const wstring &dump_path,
                                   MinidumpCallback callback,
                                   void *callback_context,
                                   bool install_handler)
    : callback_(callback), callback_context_(callback_context),
      dump_path_(dump_path), dbghelp_module_(NULL),
      minidump_write_dump_(NULL), previous_handler_(current_handler_),
      previous_filter_(NULL) {
  UpdateNextID();
  dbghelp_module_ = LoadLibrary(L"dbghelp.dll");
  if (dbghelp_module_) {
    minidump_write_dump_ = reinterpret_cast<MiniDumpWriteDump_type>(
        GetProcAddress(dbghelp_module_, "MiniDumpWriteDump"));
  }
  if (install_handler) {
    previous_filter_ = SetUnhandledExceptionFilter(HandleException);
    current_handler_ = this;
  }
}

ExceptionHandler::~ExceptionHandler() {
  if (dbghelp_module_) {
    FreeLibrary(dbghelp_module_);
  }
  if (current_handler_ == this) {
    SetUnhandledExceptionFilter(previous_filter_);
    current_handler_ = previous_handler_;
  }
}

// static
LONG ExceptionHandler::HandleException(EXCEPTION_POINTERS *exinfo) {
  if (!current_handler_->WriteMinidumpWithException(exinfo)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}

bool ExceptionHandler::WriteMinidump() {
  bool success = WriteMinidumpWithException(NULL);
  UpdateNextID();
  return success;
}

// static
bool ExceptionHandler::WriteMinidump(const wstring &dump_path,
                                     MinidumpCallback callback,
                                     void *callback_context) {
  ExceptionHandler handler(dump_path, callback, callback_context, false);
  return handler.WriteMinidump();
}

bool ExceptionHandler::WriteMinidumpWithException(EXCEPTION_POINTERS *exinfo) {
  wchar_t dump_file_name[MAX_PATH];
  swprintf_s(dump_file_name, MAX_PATH, L"%s\\%s.dmp",
             dump_path_.c_str(), next_minidump_id_.c_str());

  bool success = false;
  if (minidump_write_dump_) {
    HANDLE dump_file = CreateFile(dump_file_name,
                                  GENERIC_WRITE,
                                  FILE_SHARE_WRITE,
                                  NULL,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    if (dump_file != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION except_info;
      except_info.ThreadId = GetCurrentThreadId();
      except_info.ExceptionPointers = exinfo;
      except_info.ClientPointers = FALSE;

      // The explicit comparison to TRUE avoids a warning (C4800).
      success = (minidump_write_dump_(GetCurrentProcess(),
                                      GetCurrentProcessId(),
                                      dump_file,
                                      MiniDumpNormal,
                                      &except_info,
                                      NULL,
                                      NULL) == TRUE);

      CloseHandle(dump_file);
    }
  }

  if (callback_) {
    callback_(next_minidump_id_, callback_context_, success);
  }
  // TODO(bryner): log an error on failure

  return success;
}

void ExceptionHandler::UpdateNextID() {
  GUID id;
  CoCreateGuid(&id);
  next_minidump_id_ = GUIDString::GUIDToWString(&id);
}

}  // namespace google_airbag
