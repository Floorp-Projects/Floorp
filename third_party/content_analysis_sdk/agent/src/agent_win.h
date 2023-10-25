// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_AGENT_WIN_H_
#define CONTENT_ANALYSIS_AGENT_SRC_AGENT_WIN_H_

#include <windows.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "agent_base.h"

namespace content_analysis {
namespace sdk {

// Agent implementaton for Windows.
class AgentWin : public AgentBase {
 public:
  // Creates a new agent given the specific configuration and handler.
  // If an error occurs during creation, `rc` is set to the specific
  // error.  Otherwise `rc` is ResultCode::OK.
  AgentWin(Config config, std::unique_ptr<AgentEventHandler> handler,
           ResultCode* rc);
  ~AgentWin() override;

  // Agent:
  ResultCode HandleEvents() override;
  ResultCode Stop() override;
  std::string DebugString() const override;

  // Handles one pipe event and returns.  Used only in tests.
  ResultCode HandleOneEventForTesting();

  // Returns true if there is at least one client connected to this agent.
  bool IsAClientConnectedForTesting();

private:
  // Represents one connection to a Google Chrome browser, or one pipe
  // listening for a Google Chrome browser to connect.
  class Connection {
   public:
    // Starts listening on a pipe with the given name. `is_first_pipe` should
    // be true only for the first pipe created by the agent.  If an error
    // occurs while creating the connction object it is returned in `rc`.
    // If no errors occur then rc is set to ResultCode::OK.
    //
    // When `user_specific` is true there is a different agent instance per OS
    // user.
    //
    // `Connection` objects cannot be copied or moved because the OVERLAPPED
    // structure cannot be changed or moved in memory while an I/O operation
    // is in progress.
    Connection(const std::string& pipename,
               bool user_specific,
               AgentEventHandler* handler,
               bool is_first_pipe,
               ResultCode* rc);
    Connection(const Connection& other) = delete;
    Connection(Connection&& other) = delete;
    Connection& operator=(const Connection& other) = delete;
    Connection& operator=(Connection&& other) = delete;
    ~Connection();

    bool IsValid() const { return handle_ != INVALID_HANDLE_VALUE; }
    bool IsConnected() const { return is_connected_; }
    HANDLE GetWaitHandle() const { return overlapped_.hEvent; }

    // Resets this connection object to listen for a new Google Chrome browser.
    // When `user_specific` is true there is a different agent instance per OS
    // user.
    ResultCode Reset(const std::string& pipename, bool user_specific);

    // Hnadles an event for this connection.  `wait_handle` corresponds to
    // this connections wait handle.
    ResultCode HandleEvent(HANDLE wait_handle);

    // Append debuf information to the string stream.
    void AppendDebugString(std::stringstream& state) const;

   private:
    // Listens for a new connection from Google Chrome.
    ResultCode ConnectPipe();

    // Resets this connection object to listen for a new Google Chrome browser.
    // When `user_specific` is true there is a different agent instance per OS
    // user.
    ResultCode ResetInternal(const std::string& pipename,
                             bool user_specific,
                             bool is_first_pipe);

    // Cleans up this connection object so that it can be reused with a new
    // Google Chroem browser instance.  The handles assocated with this object
    // are not closed.  On return, this object is neither connected nor
    // listening and any buffer used to hold browser messages are cleared.
    void Cleanup();

    // Queues a read on the pipe to receive a message from Google Chrome.
    // ERROR_SUCCESS, ERROR_IO_PENDING, and ERROR_MORE_DATA are successful
    // return values. Other values represent an error with the connection.
    // If `reset_cursor` is true the internal read buffer cursor is reset to
    // the start of the buffer, otherwise it is unchanged.
    ResultCode QueueReadFile(bool reset_cursor);

    // Called when data from Google Chrome is available for reading from the
    // pipe. ERROR_SUCCESS and ERROR_MORE_DATA are both successful return
    // values.  Other values represent an error with the connection.
    //
    // `done_reading` is true if the code has finished reading an entire message
    // from chrome.  Regardless of whether reading is done, `count` contains
    // the number of bytes read.
    //
    // If `done_reading` is true, the data received from the browser is parsed
    // as if it were a `ChromeToAgent` proto message and the handler is called
    // as needed.
    //
    // If `done_reading` is false, the data received from the browser is
    // appended to the data already received from the browser.  `buffer_` is
    // resized to allow reading more data from the browser.
    //
    // In all cases the caller is expected to use QueueReadFile() to continue
    // reading data from the browser.
    ResultCode OnReadFile(BOOL done_reading, DWORD count);

    // Calls the appropriate method the handler depending on the message
    // received from Google Chrome.
    ResultCode CallHandler();

    // Fills in the browser_info_ member of this Connection.  Assumes
    // IsConnected() is true.
    ResultCode BuildBrowserInfo();

    // Notifies the handler of the given error iff `rc` is not equal to
    // ResultCode::OK.  Appends the Google Chrome browser process id to the
    // context before calling the handler.  Also append `err` to the context
    // if it is not ERROR_SUCCESS.
    //
    // Returns the error passed into the method.
    ResultCode NotifyIfError(const char* context,
                             ResultCode rc,
                             DWORD err=ERROR_SUCCESS);

    // The handler to call for various agent events.
    AgentEventHandler* handler_ = nullptr;

    // Members used to communicate with Google Chrome.
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped_;

    // True if this connection is assigned to a specific Google Chrome browser,
    // otherwise this connection is listening for a new browser.
    bool is_connected_ = false;

    // Information about the Google Chrome browser process.
    BrowserInfo browser_info_;

    // Members used to read messages from Google Chrome.
    std::vector<char> buffer_;
    char* cursor_ = nullptr;
    DWORD read_size_ = 0;
    DWORD final_size_ = 0;
  };

  // Returns handles that can be used to wait for events from all handles
  // managed by this agent.  This includes all connection objects and the
  // stop event.  The stop event is always last in the list.
  void GetHandles(std::vector<HANDLE>& wait_handles) const;

  // Handles one pipe event and returns.  If the return value is
  // ResultCode::OK, the `stopped` argument is set to true if the agent
  // should stop handling more events.  If the return value is not
  // ResultCode::OK, `stopped` is undefined.
  ResultCode HandleOneEvent(std::vector<HANDLE>& wait_handles, bool* stopped);

  // Performs a clean shutdown of the agent.
  void Shutdown();

  // Name used to create the pipes between the agent and Google Chrome browsers.
  std::string pipename_;

  // A list of pipes to already connected Google Chrome browsers.
  // The first kMinNumListeningPipeInstances pipes in the list correspond to
  // listening pipes.
  std::vector<std::unique_ptr<Connection>> connections_;

  // An event that is set when the agent should stop.  Set in Stop().
  HANDLE stop_event_ = nullptr;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_AGENT_WIN_H_
