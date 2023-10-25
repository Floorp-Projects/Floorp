// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ios>
#include <iostream>
#include <sstream>
#include <utility>

#include "event_win.h"

#include "common/utils_win.h"

#include "agent_utils_win.h"
#include "scoped_print_handle_win.h"

namespace content_analysis {
namespace sdk {

// Writes a string to the pipe. Returns ERROR_SUCCESS if successful, else
// returns GetLastError() of the write.  This function does not return until
// the entire message has been sent (or an error occurs).
static DWORD WriteMessageToPipe(HANDLE pipe, const std::string& message) {
  if (message.empty()) {
    return ERROR_SUCCESS;
  }

  internal::ScopedOverlapped overlapped;
  if (!overlapped.is_valid()) {
    return GetLastError();
  }

  DWORD err = ERROR_SUCCESS;
  const char* cursor = message.data();
  for (DWORD size = message.length(); size > 0;) {
    if (WriteFile(pipe, cursor, size, /*written=*/nullptr, overlapped)) {
      err = ERROR_SUCCESS;
      break;
    }

    // If an I/O is not pending, return the error.
    err = GetLastError();
    if (err != ERROR_IO_PENDING) {
      break;
    }

    DWORD written;
    if (!GetOverlappedResult(pipe, overlapped, &written, /*wait=*/TRUE)) {
      err = GetLastError();
      break;
    }

    cursor += written;
    size -= written;
  }

  return err;
}


ContentAnalysisEventWin::ContentAnalysisEventWin(
    HANDLE handle,
    const BrowserInfo& browser_info,
    ContentAnalysisRequest req)
    : ContentAnalysisEventBase(browser_info),
      hPipe_(handle) {
  *request() = std::move(req);
}

ContentAnalysisEventWin::~ContentAnalysisEventWin() {
  Shutdown();
}

ResultCode ContentAnalysisEventWin::Init() {
  // TODO(rogerta): do some extra validation of the request?
  if (request()->request_token().empty()) {
    return ResultCode::ERR_MISSING_REQUEST_TOKEN;
  }

  response()->set_request_token(request()->request_token());

  // Prepare the response so that ALLOW verdicts are the default().
  return UpdateResponse(*response(),
      request()->tags_size() > 0 ? request()->tags(0) : std::string(),
      ContentAnalysisResponse::Result::SUCCESS);
}

ResultCode ContentAnalysisEventWin::Close() {
  Shutdown();
  return ContentAnalysisEventBase::Close();
}

ResultCode ContentAnalysisEventWin::Send() {
  if (response_sent_) {
    return ResultCode::ERR_RESPONSE_ALREADY_SENT;
  }

  response_sent_ = true;

  DWORD err = WriteMessageToPipe(hPipe_,
                                 agent_to_chrome()->SerializeAsString());
  return ErrorToResultCode(err);
}

std::string ContentAnalysisEventWin::DebugString() const {
  std::stringstream state;
  state.setf(std::ios::boolalpha);
  state << "ContentAnalysisEventWin{handle=" << hPipe_;
  state << " pid=" << GetBrowserInfo().pid;
  state << " rtoken=" << GetRequest().request_token();
  state << " sent="  << response_sent_;
  state << "}" << std::ends;

  return state.str();
}

void ContentAnalysisEventWin::Shutdown() {
  if (hPipe_ != INVALID_HANDLE_VALUE) {
    // If no response has been sent yet, attempt to send it now.  Otherwise
    // the client may be stuck waiting.  After shutdown the agent will not
    // have any other chance to respond.
    if (!response_sent_) {
      Send();
    }

    // This event does not own the pipe, so don't close it.
    FlushFileBuffers(hPipe_);
    hPipe_ = INVALID_HANDLE_VALUE;
  }
}

}  // namespace sdk
}  // namespace content_analysis
