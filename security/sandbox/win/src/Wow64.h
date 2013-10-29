// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WOW64_H__
#define SANDBOX_SRC_WOW64_H__

#include <windows.h>

#include "base/basictypes.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class TargetProcess;

// This class wraps the code needed to interact with the Windows On Windows
// subsystem on 64 bit OSes, from the point of view of interceptions.
class Wow64 {
 public:
  Wow64(TargetProcess* child, HMODULE ntdll)
      : child_(child), ntdll_(ntdll), dll_load_(NULL), continue_load_(NULL) {}
  ~Wow64();

  // Waits for the 32 bit DLL to get loaded on the child process. This function
  // will return immediately if not running under WOW, or launch the helper
  // process and wait until ntdll is ready.
  bool WaitForNtdll();

 private:
  // Runs the WOW helper process, passing the address of a buffer allocated on
  // the child (one page).
  bool RunWowHelper(void* buffer);

  // This method receives "notifications" whenever a DLL is mapped on the child.
  bool DllMapped();

  // Returns true if ntdll.dll is mapped on the child.
  bool NtdllPresent();

  TargetProcess* child_;  // Child process.
  HMODULE ntdll_;         // ntdll on the parent.
  HANDLE dll_load_;       // Event that is signaled on dll load.
  HANDLE continue_load_;  // Event to signal to continue execution on the child.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Wow64);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_WOW64_H__
