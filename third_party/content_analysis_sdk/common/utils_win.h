// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility and helper functions common to both the agent and browser code.
// This code is not publicly exposed from the SDK.

#ifndef CONTENT_ANALYSIS_COMMON_UTILS_WIN_H_
#define CONTENT_ANALYSIS_COMMON_UTILS_WIN_H_

#include <string>

namespace content_analysis {
namespace sdk {
namespace internal {

// The default size of the buffer used to hold messages received from
// Google Chrome.
const DWORD kBufferSize = 4096;

// Named pipe prefixes used for agent and client side of pipe.
constexpr char kPipePrefixForAgent[] = R"(\\.\\pipe\)";
constexpr char kPipePrefixForClient[] = R"(\Device\NamedPipe\)";

// Returns the user SID of the thread or process that calls thie function.
// Returns an empty string on error.
std::string GetUserSID();

// Returns the name of the pipe that should be used to communicate between
// the agent and Google Chrome.  If `sid` is non-empty, make the pip name
// specific to that user.
//
// GetPipeNameForAgent() is meant to be used in the agent.  The returned
// path can be used with CreatePipe() below.  GetPipeNameForClient() is meant
// to be used in the client.  The returned path can only be used with
// NtCreateFile() and not CreateFile().
std::string GetPipeNameForAgent(const std::string& base, bool user_specific);
std::string GetPipeNameForClient(const std::string& base, bool user_specific);

// Creates a named pipe with the give name.  If `is_first_pipe` is true,
// fail if this is not the first pipe using this name.
//
// This function create a pipe whose DACL allow full control to the creator
// owner and administrators.  If `user_specific` the DACL only allows the
// logged on user to read from and write to the pipe.  Otherwise anyone logged
// in can read from and write to the pipe.
//
// A handle to the pipe is retuned in `handle`.
DWORD CreatePipe(const std::string& name,
                 bool user_specific,
                 bool is_first_pipe,
                 HANDLE* handle);

// Returns the full path to the main binary file of the process with the given
// process ID.
bool GetProcessPath(unsigned long pid, std::string* binary_path);

// A class that scopes the creation and destruction of an OVERLAPPED structure
// used for async IO.
class ScopedOverlapped {
 public:
  ScopedOverlapped();
  ~ScopedOverlapped();

  bool is_valid() { return overlapped_.hEvent != nullptr; }
  operator OVERLAPPED*() { return &overlapped_; }

 private:
  OVERLAPPED overlapped_;
};

}  // internal
}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_COMMON_UTILS_WIN_H_