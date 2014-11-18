// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_TYPES_H_
#define SANDBOX_WIN_SRC_SANDBOX_TYPES_H_

namespace sandbox {

// Operation result codes returned by the sandbox API.
enum ResultCode {
  SBOX_ALL_OK = 0,
  // Error is originating on the win32 layer. Call GetlastError() for more
  // information.
  SBOX_ERROR_GENERIC = 1,
  // An invalid combination of parameters was given to the API.
  SBOX_ERROR_BAD_PARAMS = 2,
  // The desired operation is not supported at this time.
  SBOX_ERROR_UNSUPPORTED = 3,
  // The request requires more memory that allocated or available.
  SBOX_ERROR_NO_SPACE = 4,
  // The ipc service requested does not exist.
  SBOX_ERROR_INVALID_IPC = 5,
  // The ipc service did not complete.
  SBOX_ERROR_FAILED_IPC = 6,
  // The requested handle was not found.
  SBOX_ERROR_NO_HANDLE = 7,
  // This function was not expected to be called at this time.
  SBOX_ERROR_UNEXPECTED_CALL = 8,
  // WaitForAllTargets is already called.
  SBOX_ERROR_WAIT_ALREADY_CALLED = 9,
  // A channel error prevented DoCall from executing.
  SBOX_ERROR_CHANNEL_ERROR = 10,
  // Failed to create the alternate desktop.
  SBOX_ERROR_CANNOT_CREATE_DESKTOP = 11,
  // Failed to create the alternate window station.
  SBOX_ERROR_CANNOT_CREATE_WINSTATION = 12,
  // Failed to switch back to the interactive window station.
  SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION = 13,
  // The supplied AppContainer is not valid.
  SBOX_ERROR_INVALID_APP_CONTAINER = 14,
  // The supplied capability is not valid.
  SBOX_ERROR_INVALID_CAPABILITY = 15,
  // There is a failure initializing the AppContainer.
  SBOX_ERROR_CANNOT_INIT_APPCONTAINER = 16,
  // Initializing or updating ProcThreadAttributes failed.
  SBOX_ERROR_PROC_THREAD_ATTRIBUTES = 17,
  // Placeholder for last item of the enum.
  SBOX_ERROR_LAST
};

// If the sandbox cannot create a secure environment for the target, the
// target will be forcibly terminated. These are the process exit codes.
enum TerminationCodes {
  SBOX_FATAL_INTEGRITY = 7006,       // Could not set the integrity level.
  SBOX_FATAL_DROPTOKEN = 7007,       // Could not lower the token.
  SBOX_FATAL_FLUSHANDLES = 7008,     // Failed to flush registry handles.
  SBOX_FATAL_CACHEDISABLE = 7009,    // Failed to forbid HCKU caching.
  SBOX_FATAL_CLOSEHANDLES = 7010,    // Failed to close pending handles.
  SBOX_FATAL_MITIGATION = 7011,      // Could not set the mitigation policy.
  SBOX_FATAL_MEMORY_EXCEEDED = 7012, // Exceeded the job memory limit.
  SBOX_FATAL_LAST
};

class BrokerServices;
class TargetServices;

// Contains the pointer to a target or broker service.
struct SandboxInterfaceInfo {
  BrokerServices* broker_services;
  TargetServices* target_services;
};

#if SANDBOX_EXPORTS
#define SANDBOX_INTERCEPT extern "C" __declspec(dllexport)
#else
#define SANDBOX_INTERCEPT extern "C"
#endif

enum InterceptionType {
  INTERCEPTION_INVALID = 0,
  INTERCEPTION_SERVICE_CALL,    // Trampoline of an NT native call
  INTERCEPTION_EAT,
  INTERCEPTION_SIDESTEP,        // Preamble patch
  INTERCEPTION_SMART_SIDESTEP,  // Preamble patch but bypass internal calls
  INTERCEPTION_UNLOAD_MODULE,   // Unload the module (don't patch)
  INTERCEPTION_LAST             // Placeholder for last item in the enumeration
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_TYPES_H_
