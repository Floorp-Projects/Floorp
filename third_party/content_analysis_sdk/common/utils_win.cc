// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <windows.h>
#include <sddl.h>

#include "utils_win.h"

namespace content_analysis {
namespace sdk {
namespace internal {

std::string GetUserSID() {
  std::string sid;

  HANDLE handle;
  if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &handle) &&
    !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &handle)) {
    return std::string();
  }

  DWORD length = 0;
  std::vector<char> buffer;
  if (!GetTokenInformation(handle, TokenUser, nullptr, 0, &length)) {
    DWORD err = GetLastError();
    if (err == ERROR_INSUFFICIENT_BUFFER) {
      buffer.resize(length);
    }
  }
  if (GetTokenInformation(handle, TokenUser, buffer.data(), buffer.size(),
    &length)) {
    TOKEN_USER* info = reinterpret_cast<TOKEN_USER*>(buffer.data());
    char* sid_string;
    if (ConvertSidToStringSidA(info->User.Sid, &sid_string)) {
      sid = sid_string;
      LocalFree(sid_string);
    }
  }

  CloseHandle(handle);
  return sid;
}

std::string BuildPipeName(const char* prefix,
                          const std::string& base,
                          bool user_specific) {
  std::string pipename = prefix;

  // If the agent is not user-specific, the assumption is that it runs with
  // administrator privileges.  Create the pipe in a location only available
  // to administrators.
  if (!user_specific)
    pipename += "ProtectedPrefix\\Administrators\\";

  pipename += base;

  if (user_specific) {
    std::string sid = GetUserSID();
    if (sid.empty())
      return std::string();

    pipename += "." + sid;
  }

  return pipename;
}

std::string GetPipeNameForAgent(const std::string& base, bool user_specific) {
  return BuildPipeName(kPipePrefixForAgent, base, user_specific);
}

std::string GetPipeNameForClient(const std::string& base, bool user_specific) {
  return BuildPipeName(kPipePrefixForClient, base, user_specific);
}

DWORD CreatePipe(
    const std::string& name,
    bool user_specific,
    bool is_first_pipe,
    HANDLE* handle) {
  DWORD err = ERROR_SUCCESS;
  DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;

  // Create DACL for pipe to allow users on the local system to read and write
  // to the pipe.  The creator and owner as well as administrator always get
  // full control of the pipe.
  //
  // If `user_specific` is true, a different agent instance is used for each
  // OS user, so only allow the interactive logged on user to reads and write
  // messages to the pipe.  Otherwise only one agent instance used used for all
  // OS users and all authenticated logged on users can reads and write
  // messages.
  //
  // See https://docs.microsoft.com/en-us/windows/win32/secauthz/security-descriptor-definition-language
  // for a description of this string format.
  constexpr char kDaclEveryone[] = "D:"
      "(A;OICI;GA;;;CO)"     // Allow full control to creator owner.
      "(A;OICI;GA;;;BA)"     // Allow full control to admins.
      "(A;OICI;GRGW;;;WD)";  // Allow read and write to everyone.
  constexpr char kDaclUserSpecific[] = "D:"
      "(A;OICI;GA;;;CO)"     // Allow full control to creator owner.
      "(A;OICI;GA;;;BA)"     // Allow full control to admins.
      "(A;OICI;GRGW;;;IU)";  // Allow read and write to interactive user.
  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = FALSE;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
      user_specific ? kDaclUserSpecific : kDaclEveryone, SDDL_REVISION_1,
      &sa.lpSecurityDescriptor, /*outSdSize=*/nullptr)) {
    err = GetLastError();
    return err;
  }

  // When `is_first_pipe` is true, the agent expects there is no process that
  // is currently listening on this pipe.  If there is, CreateNamedPipeA()
  // returns with ERROR_ACCESS_DENIED.  This is used to detect if another
  // process is listening for connections when there shouldn't be.
  if (is_first_pipe) {
    mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
  }
  *handle = CreateNamedPipeA(name.c_str(), mode,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
    PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, kBufferSize,
    kBufferSize, 0, &sa);
  if (*handle == INVALID_HANDLE_VALUE) {
    err = GetLastError();
  }

  // Free the security descriptor as it is no longer needed once
  // CreateNamedPipeA() returns.
  LocalFree(sa.lpSecurityDescriptor);

  return err;
}

bool GetProcessPath(unsigned long pid, std::string* binary_path) {
  HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (hProc == nullptr) {
    return false;
  }
  
  char path[MAX_PATH];
  DWORD size = sizeof(path);
  DWORD length = QueryFullProcessImageNameA(hProc, /*flags=*/0, path, &size);
  CloseHandle(hProc);
  if (length == 0) {
    return false;
  }

  *binary_path = path;
  return true;
}

ScopedOverlapped::ScopedOverlapped() {
  memset(&overlapped_, 0, sizeof(overlapped_));
  overlapped_.hEvent = CreateEvent(/*securityAttr=*/nullptr,
                                   /*manualReset=*/TRUE,
                                   /*initialState=*/FALSE,
                                   /*name=*/nullptr);
}

ScopedOverlapped::~ScopedOverlapped() {
  if (overlapped_.hEvent != nullptr) {
    CloseHandle(overlapped_.hEvent);
  }
}

}  // internal
}  // namespace sdk
}  // namespace content_analysis
