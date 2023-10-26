// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <utility>
#include <vector>

#include <windows.h>
#include <sddl.h>

#include "common/utils_win.h"

#include "agent_utils_win.h"
#include "agent_win.h"
#include "event_win.h"

namespace content_analysis {
namespace sdk {

// The minimum number of pipe in listening mode.  This is greater than one to
// handle the case of multiple instance of Google Chrome browser starting
// at the same time.
const DWORD kMinNumListeningPipeInstances = 2;

// The minimum number of handles to wait on.  This is the minimum number
// of pipes in listening mode plus the stop event.
const DWORD kMinNumWaitHandles = kMinNumListeningPipeInstances + 1;

// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler,
    ResultCode* rc) {
  auto agent = std::make_unique<AgentWin>(std::move(config), std::move(handler), rc);
  return *rc == ResultCode::OK ? std::move(agent) : nullptr;
}

AgentWin::Connection::Connection(const std::string& pipename,
                                 bool user_specific,
                                 AgentEventHandler* handler,
                                 bool is_first_pipe,
                                 ResultCode* rc)
    : handler_(handler)  {
  *rc = ResultCode::OK;
  memset(&overlapped_, 0, sizeof(overlapped_));
  // Create a manual reset event as specified for overlapped IO.
  // Use default security attriutes and no name since this event is not
  // shared with other processes.
  overlapped_.hEvent = CreateEvent(/*securityAttr=*/nullptr,
                                   /*manualReset=*/TRUE,
                                   /*initialState=*/FALSE,
                                   /*name=*/nullptr);
  if (!overlapped_.hEvent) {
    *rc = ResultCode::ERR_CANNOT_CREATE_CHANNEL_IO_EVENT;
    return;
  }

  *rc = ResetInternal(pipename, user_specific, is_first_pipe);
}

AgentWin::Connection::~Connection() {
  Cleanup();

  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }

  // Invalid event handles are represented as null.
  if (overlapped_.hEvent) {
    CloseHandle(overlapped_.hEvent);
  }
}

ResultCode AgentWin::Connection::Reset(
    const std::string& pipename,
    bool user_specific) {
  return NotifyIfError("ConnectionReset",
                       ResetInternal(pipename, user_specific, false));
}

ResultCode AgentWin::Connection::HandleEvent(HANDLE handle) {
  auto rc = ResultCode::OK;
  DWORD count;
  BOOL success = GetOverlappedResult(handle, &overlapped_, &count,
                                     /*wait=*/FALSE);
  if (!is_connected_) {
    // This connection is currently listing for a new connection from a Google
    // Chrome browser.  If the result is a success, this means the browser has
    // connected as expected.  Otherwise an error occured so report it to the
    // caller.
    if (success) {
      // A Google Chrome browser connected to the agent.  Reset this
      // connection object to handle communication with the browser and then
      // tell the handler about it.

      is_connected_ = true;
      buffer_.resize(internal::kBufferSize);

      rc = BuildBrowserInfo();
      if (rc == ResultCode::OK) {
        handler_->OnBrowserConnected(browser_info_);
      }
    } else {
      rc = ErrorToResultCode(GetLastError());
      NotifyIfError("GetOverlappedResult", rc);
    }
  } else {
    // Some data has arrived from Google Chrome. This data is (part of) an
    // instance of the proto message `ChromeToAgent`.
    //
    // If the message is small it is received in by one call to ReadFile().
    // If the message is larger it is received in by multiple calls to
    // ReadFile().
    //
    // `success` is true if the data just read is the last bytes for a message.
    // Otherwise it is false.
    rc = OnReadFile(success, count);
  }

  // If all data has been read, queue another read.
  if (rc == ResultCode::OK || rc == ResultCode::ERR_MORE_DATA) {
    rc = QueueReadFile(rc == ResultCode::OK);
  }

  if (rc != ResultCode::OK && rc != ResultCode::ERR_IO_PENDING &&
      rc != ResultCode::ERR_MORE_DATA) {
    Cleanup();
  } else {
    // Don't propagate all the "success" error codes to the called to keep
    // this simpler.
    rc = ResultCode::OK;
  }

  return rc;
}

void AgentWin::Connection::AppendDebugString(std::stringstream& state) const {
  state << "{handle=" << handle_;
  state << " connected=" << is_connected_;
  state << " pid=" << browser_info_.pid;
  state << " rsize=" << read_size_;
  state << " fsize=" << final_size_;
  state << "}";
}

ResultCode AgentWin::Connection::ConnectPipe() {
  // In overlapped mode, connecting to a named pipe always returns false.
  if (ConnectNamedPipe(handle_, &overlapped_)) {
    return ErrorToResultCode(GetLastError());
  }

  DWORD err = GetLastError();
  if (err == ERROR_IO_PENDING) {
    // Waiting for a Google Chrome Browser to connect.
    return ResultCode::OK;
  } else if (err == ERROR_PIPE_CONNECTED) {
    // A Google Chrome browser is already connected.  Make sure event is in
    // signaled state in order to process the connection.
    if (SetEvent(overlapped_.hEvent)) {
      err = ERROR_SUCCESS;
    } else {
      err = GetLastError();
    }
  }

  return ErrorToResultCode(err);
}

ResultCode AgentWin::Connection::ResetInternal(const std::string& pipename,
                                               bool user_specific,
                                               bool is_first_pipe) {
  auto rc = ResultCode::OK;

  // If this is the not the first time, disconnect from any existing Google
  // Chrome browser.  Otherwise creater a new pipe.
  if (handle_ != INVALID_HANDLE_VALUE) {
    if (!DisconnectNamedPipe(handle_)) {
      rc = ErrorToResultCode(GetLastError());
    }
  } else {
    rc = ErrorToResultCode(
        internal::CreatePipe(pipename, user_specific, is_first_pipe, &handle_));
  }

  // Make sure event starts in reset state.
  if (rc == ResultCode::OK && !ResetEvent(overlapped_.hEvent)) {
    rc = ErrorToResultCode(GetLastError());
  }

  if (rc == ResultCode::OK) {
    rc = ConnectPipe();
  }

  if (rc != ResultCode::OK) {
    Cleanup();
    handle_ = INVALID_HANDLE_VALUE;
  }

  return rc;
}

void AgentWin::Connection::Cleanup() {
  if (is_connected_ && handler_) {
    handler_->OnBrowserDisconnected(browser_info_);
  }

  is_connected_ = false;
  browser_info_ = BrowserInfo();
  buffer_.clear();
  cursor_ = nullptr;
  read_size_ = 0;
  final_size_ = 0;

  if (handle_ != INVALID_HANDLE_VALUE) {
    // Cancel all outstanding IO requests on this pipe by using a null for
    // overlapped.
    CancelIoEx(handle_, /*overlapped=*/nullptr);
  }

  // This function does not close `handle_` or the event in `overlapped` so
  // that the server can resuse the pipe with an new Google Chrome browser
  // instance.
}

ResultCode AgentWin::Connection::QueueReadFile(bool reset_cursor) {
  if (reset_cursor) {
    cursor_ = buffer_.data();
    read_size_ = buffer_.size();
    final_size_ = 0;
  }

  // When this function is called there are the following possiblities:
  //
  // 1/ Data is already available and the buffer is filled in.  ReadFile()
  //    return TRUE and the event is set.
  // 2/ Data is not avaiable yet.  ReadFile() returns FALSE and the last error
  //    is ERROR_IO_PENDING(997) and the event is reset.
  // 3/ Some error occurred, like for example Google Chrome stops.  ReadFile()
  //    returns FALSE and the last error is something other than
  //    ERROR_IO_PENDING, for example ERROR_BROKEN_PIPE(109).  The event
  //    state is unchanged.
  auto rc = ResultCode::OK;
  DWORD count;
  if (!ReadFile(handle_, cursor_, read_size_, &count, &overlapped_)) {
    DWORD err = GetLastError();
    rc = ErrorToResultCode(err);

    // IO pending is not an error so don't notify.
    //
    // Ignore broken pipes for notifications since that happens when the Google
    // Chrome browser shuts down.  The agent will be notified of a browser
    // disconnect in that case.
    //
    // More data means that `buffer_` was too small to read the entire message
    // from the browser.  The buffer has already been resized.  Another call to
    // ReadFile() is needed to get the remainder.
    if (rc != ResultCode::ERR_IO_PENDING &&
        rc != ResultCode::ERR_BROKEN_PIPE &&
        rc != ResultCode::ERR_MORE_DATA) {
      NotifyIfError("QueueReadFile", rc, err);
    }
  }

  return rc;
}

ResultCode AgentWin::Connection::OnReadFile(BOOL done_reading, DWORD count) {
  final_size_ += count;

  // If `done_reading` is TRUE, this means the full message has been read.
  // Call the appropriate handler method.
  if (done_reading) {
    return CallHandler();
  }

  // Otherwise there are two possibilities:
  //
  // 1/ The last error is ERROR_MORE_DATA(234).  This means there are more
  //    bytes to read before the request message is complete.  Resize the
  //    buffer and adjust the cursor.  The caller will queue up another read
  //    and wait.  don't notify the handler since this is not an error.
  // 2/ Some error occured.  In this case notify the handler and return the
  //    error.

  DWORD err = GetLastError();
  if (err == ERROR_MORE_DATA) {
    read_size_ = internal::kBufferSize;
    buffer_.resize(buffer_.size() + read_size_);
    cursor_ = buffer_.data() + buffer_.size() - read_size_;
    return ErrorToResultCode(err);
  }

  return NotifyIfError("OnReadFile", ErrorToResultCode(err));
}

ResultCode AgentWin::Connection::CallHandler() {
  ChromeToAgent message;
  if (!message.ParseFromArray(buffer_.data(), final_size_)) {
    // Malformed message.
    return NotifyIfError("ParseChromeToAgent",
                         ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER);
  }

  auto rc = ResultCode::OK;

  if (message.has_request()) {
    // This is a request from Google Chrome to perform a content analysis
    // request.
    //
    // Move the request from `message` to the event to reduce the amount
    // of memory allocation/copying and also because the the handler takes
    // ownership of the event.
    auto event = std::make_unique<ContentAnalysisEventWin>(
        handle_, browser_info_, std::move(*message.mutable_request()));
    rc = event->Init();
    if (rc == ResultCode::OK) {
      handler_->OnAnalysisRequested(std::move(event));
    } else {
      NotifyIfError("RequestValidation", rc);
    }
  } else if (message.has_ack()) {
    // This is an ack from Google Chrome that it has received a content
    // analysis response from the agent.
    handler_->OnResponseAcknowledged(message.ack());
  } else if (message.has_cancel()) {
    // Google Chrome is informing the agent that the content analysis
    // request(s) associated with the given user action id have been
    // canceled by the user.
    handler_->OnCancelRequests(message.cancel());
  } else {
    // Malformed message.
    rc = NotifyIfError("NoRequestOrAck",
                       ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER);
  }

  return rc;
}

ResultCode AgentWin::Connection::BuildBrowserInfo() {
  if (!GetNamedPipeClientProcessId(handle_, &browser_info_.pid)) {
    return NotifyIfError("BuildBrowserInfo",
                         ResultCode::ERR_CANNOT_GET_BROWSER_PID);
  }

  if (!internal::GetProcessPath(browser_info_.pid,
                                &browser_info_.binary_path)) {
    return NotifyIfError("BuildBrowserInfo",
                         ResultCode::ERR_CANNOT_GET_BROWSER_BINARY_PATH);
  }

  return ResultCode::OK;
}

ResultCode AgentWin::Connection::NotifyIfError(
    const char* context,
    ResultCode rc,
    DWORD err) {
  if (handler_ && rc != ResultCode::OK) {
    std::stringstream stm;
    stm << context << " pid=" << browser_info_.pid;
    if (err != ERROR_SUCCESS) {
      stm << context << " err=" << err;
    }

    handler_->OnInternalError(stm.str().c_str(), rc);
  }
  return rc;
}

AgentWin::AgentWin(
    Config config,
    std::unique_ptr<AgentEventHandler> event_handler,
    ResultCode* rc)
  : AgentBase(std::move(config), std::move(event_handler)) {
  *rc = ResultCode::OK;
  if (handler() == nullptr) {
    *rc = ResultCode::ERR_AGENT_EVENT_HANDLER_NOT_SPECIFIED;
    return;
  }

  stop_event_ = CreateEvent(/*securityAttr=*/nullptr,
                            /*manualReset=*/TRUE,
                            /*initialState=*/FALSE,
                            /*name=*/nullptr);
  if (stop_event_ == nullptr) {
    *rc = ResultCode::ERR_CANNOT_CREATE_AGENT_STOP_EVENT;
    return;
  }

  std::string pipename =
      internal::GetPipeNameForAgent(configuration().name,
                                    configuration().user_specific);
  if (pipename.empty()) {
    *rc = ResultCode::ERR_INVALID_CHANNEL_NAME;
    return;
  }

  pipename_ = pipename;

  connections_.reserve(kMinNumListeningPipeInstances);
  for (DWORD i = 0; i < kMinNumListeningPipeInstances; ++i) {
    connections_.emplace_back(
        std::make_unique<Connection>(pipename_, configuration().user_specific,
                                     handler(), i == 0, rc));
    if (*rc != ResultCode::OK || !connections_.back()->IsValid()) {
      Shutdown();
      break;
    }
  }
}

AgentWin::~AgentWin() {
  Shutdown();
}

ResultCode AgentWin::HandleEvents() {
  std::vector<HANDLE> wait_handles;
  auto rc = ResultCode::OK;
  bool stopped = false;
  while (!stopped && rc == ResultCode::OK) {
    rc = HandleOneEvent(wait_handles, &stopped);
  }

  return rc;
}

ResultCode AgentWin::Stop() {
  SetEvent(stop_event_);
  return AgentBase::Stop();
}

std::string AgentWin::DebugString() const {
  std::stringstream state;
  state.setf(std::ios::boolalpha);
  state << "AgentWin{pipe=\"" << pipename_;
  state << "\" stop=" << stop_event_;

  for (size_t i = 0; i < connections_.size(); ++i) {
    state << " conn@" << i;
    connections_[i]->AppendDebugString(state);
  }

  state << "}" << std::ends;
  return state.str();
}

void AgentWin::GetHandles(std::vector<HANDLE>& wait_handles) const {
  // Reserve enough space in the handles vector to include the stop event plus
  // all connections.
  wait_handles.clear();
  wait_handles.reserve(1 + connections_.size());

  for (auto& state : connections_) {
    HANDLE wait_handle = state->GetWaitHandle();
    if (!wait_handle) {
      wait_handles.clear();
      break;
    }
    wait_handles.push_back(wait_handle);
  }

  // Push the stop event last so that connections_ index calculations in
  // HandleOneEvent() don't have to account for this handle.
  wait_handles.push_back(stop_event_);
}

ResultCode AgentWin::HandleOneEventForTesting() {
  std::vector<HANDLE> wait_handles;
  bool stopped;
  return HandleOneEvent(wait_handles, &stopped);
}

bool AgentWin::IsAClientConnectedForTesting() {
  for (const auto& state : connections_) {
    if (state->IsConnected()) {
      return true;
    }
  }
  return false;
}

ResultCode AgentWin::HandleOneEvent(
    std::vector<HANDLE>& wait_handles,
    bool* stopped) {
  *stopped = false;

  // Wait on the specified handles for an event to occur.
  GetHandles(wait_handles);
  if (wait_handles.size() < kMinNumWaitHandles) {
    return NotifyError("GetHandles", ResultCode::ERR_AGENT_NOT_INITIALIZED);
  }

  DWORD index = WaitForMultipleObjects(
      wait_handles.size(), wait_handles.data(),
      /*waitAll=*/FALSE, /*timeoutMs=*/INFINITE);
  if (index == WAIT_FAILED) {
    return NotifyError("WaitForMultipleObjects",
                       ErrorToResultCode(GetLastError()));
  }

  // If the index of signaled handle is the last one in wait_handles, then the
  // stop event was signaled.
  index -= WAIT_OBJECT_0;
  if (index == wait_handles.size() - 1) {
    *stopped = true;
    return ResultCode::OK;
  }

  auto& connection = connections_[index];
  bool was_listening = !connection->IsConnected();
  auto rc = connection->HandleEvent(wait_handles[index]);
  if (rc != ResultCode::OK) {
    // If `connection` was not listening and there are more than
    // kMinNumListeningPipeInstances pipes, delete this connection.  Otherwise
    // reset it so that it becomes a listener.
    if (!was_listening &&
      connections_.size() > kMinNumListeningPipeInstances) {
      connections_.erase(connections_.begin() + index);
    } else {
      rc = connection->Reset(pipename_, configuration().user_specific);
    }
  }

  // If `connection` was listening and is now connected, create a new
  // one so that there are always kMinNumListeningPipeInstances listening.
  if (rc == ResultCode::OK && was_listening && connection->IsConnected()) {
    connections_.emplace_back(
        std::make_unique<Connection>(pipename_, configuration().user_specific,
                                     handler(), false, &rc));
  }

  return ResultCode::OK;
}

void AgentWin::Shutdown() {
  connections_.clear();
  pipename_.clear();
  if (stop_event_ != nullptr) {
    CloseHandle(stop_event_);
    stop_event_ = nullptr;
  }
}

}  // namespace sdk
}  // namespace content_analysis
