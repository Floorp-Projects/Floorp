// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_win.h"

namespace content_analysis {
namespace sdk {

std::unique_ptr<ScopedPrintHandle>
CreateScopedPrintHandle(const ContentAnalysisRequest& request,
                        int64_t browser_pid) {
  if (!request.has_print_data() || !request.print_data().has_handle()) {
    return nullptr;
  }

  // The handle in the request must be duped to be read by the agent
  // process. If that doesn't work for any reason, return null.
  // See https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
  // for details.
  HANDLE browser_process = OpenProcess(
      /*dwDesiredAccess=*/PROCESS_DUP_HANDLE,
      /*bInheritHandle=*/false,
      /*dwProcessId=*/browser_pid);
  if (!browser_process)
    return nullptr;

  HANDLE dupe = nullptr;
  DuplicateHandle(
      /*hSourceProcessHandle=*/browser_process,
      /*hSourceHandle=*/reinterpret_cast<HANDLE>(request.print_data().handle()),
      /*hTargetProcessHandle=*/GetCurrentProcess(),
      /*lpTargetHandle=*/&dupe,
      /*dwDesiredAccess=*/PROCESS_DUP_HANDLE | FILE_MAP_READ,
      /*bInheritHandle=*/false,
      /*dwOptions=*/0);

  CloseHandle(browser_process);

  if (!dupe)
    return nullptr;

  ContentAnalysisRequest::PrintData dupe_print_data;
  dupe_print_data.set_handle(reinterpret_cast<int64_t>(dupe));
  dupe_print_data.set_size(request.print_data().size());


  return std::make_unique<ScopedPrintHandleWin>(dupe_print_data);
}

ScopedPrintHandleWin::ScopedPrintHandleWin(
    const ContentAnalysisRequest::PrintData& print_data)
    : ScopedPrintHandleBase(print_data),
      handle_(reinterpret_cast<HANDLE>(print_data.handle())) {
  mapped_ = MapViewOfFile(handle_, FILE_MAP_READ, 0, 0, 0);
}

ScopedPrintHandleWin::~ScopedPrintHandleWin() {
  CloseHandle(handle_);
}

const char* ScopedPrintHandleWin::data() {
  return reinterpret_cast<const char*>(mapped_);
}

}  // namespace sdk
}  // namespace content_analysis
