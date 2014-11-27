// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SECURITY_LEVEL_H_
#define SANDBOX_SRC_SECURITY_LEVEL_H_

#include "base/basictypes.h"

namespace sandbox {

// List of all the integrity levels supported in the sandbox. This is used
// only on Windows Vista. You can't set the integrity level of the process
// in the sandbox to a level higher than yours.
enum IntegrityLevel {
  INTEGRITY_LEVEL_SYSTEM,
  INTEGRITY_LEVEL_HIGH,
  INTEGRITY_LEVEL_MEDIUM,
  INTEGRITY_LEVEL_MEDIUM_LOW,
  INTEGRITY_LEVEL_LOW,
  INTEGRITY_LEVEL_BELOW_LOW,
  INTEGRITY_LEVEL_UNTRUSTED,
  INTEGRITY_LEVEL_LAST
};

// The Token level specifies a set of  security profiles designed to
// provide the bulk of the security of sandbox.
//
//  TokenLevel                 |Restricting   |Deny Only       |Privileges|
//                             |Sids          |Sids            |          |
// ----------------------------|--------------|----------------|----------|
// USER_LOCKDOWN               | Null Sid     | All            | None     |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED             | RESTRICTED   | All            | Traverse |
// ----------------------------|--------------|----------------|----------|
// USER_LIMITED                | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             |              | Interactive    |          |
// ----------------------------|--------------|----------------|----------|
// USER_INTERACTIVE            | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             | Owner        | Interactive    |          |
//                             |              | Local          |          |
//                             |              | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_NON_ADMIN              | None         | All except:    | Traverse |
//                             |              | Users          |          |
//                             |              | Everyone       |          |
//                             |              | Interactive    |          |
//                             |              | Local          |          |
//                             |              | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED_SAME_ACCESS | All          | None           | All      |
// ----------------------------|--------------|----------------|----------|
// USER_UNPROTECTED            | None         | None           | All      |
// ----------------------------|--------------|----------------|----------|
//
// The above restrictions are actually a transformation that is applied to
// the existing broker process token. The resulting token that will be
// applied to the target process depends both on the token level selected
// and on the broker token itself.
//
//  The LOCKDOWN and RESTRICTED are designed to allow access to almost
//  nothing that has security associated with and they are the recommended
//  levels to run sandboxed code specially if there is a chance that the
//  broker is process might be started by a user that belongs to the Admins
//  or power users groups.
enum TokenLevel {
   USER_LOCKDOWN = 0,
   USER_RESTRICTED,
   USER_LIMITED,
   USER_INTERACTIVE,
   USER_NON_ADMIN,
   USER_RESTRICTED_SAME_ACCESS,
   USER_UNPROTECTED,
   USER_LAST
};

// The Job level specifies a set of decreasing security profiles for the
// Job object that the target process will be placed into.
// This table summarizes the security associated with each level:
//
//  JobLevel        |General                            |Quota               |
//                  |restrictions                       |restrictions        |
// -----------------|---------------------------------- |--------------------|
// JOB_NONE         | No job is assigned to the         | None               |
//                  | sandboxed process.                |                    |
// -----------------|---------------------------------- |--------------------|
// JOB_UNPROTECTED  | None                              | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// JOB_INTERACTIVE  | *Forbid system-wide changes using |                    |
//                  |  SystemParametersInfo().          | *Kill on Job close.|
//                  | *Forbid the creation/switch of    |                    |
//                  |  Desktops.                        |                    |
//                  | *Forbids calls to ExitWindows().  |                    |
// -----------------|---------------------------------- |--------------------|
// JOB_LIMITED_USER | Same as INTERACTIVE_USER plus:    | *One active process|
//                  | *Forbid changes to the display    |  limit.            |
//                  |  settings.                        | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// JOB_RESTRICTED   | Same as LIMITED_USER plus:        | *One active process|
//                  | * No read/write to the clipboard. |  limit.            |
//                  | * No access to User Handles that  | *Kill on Job close.|
//                  |   belong to other processes.      |                    |
//                  | * Forbid message broadcasts.      |                    |
//                  | * Forbid setting global hooks.    |                    |
//                  | * No access to the global atoms   |                    |
//                  |   table.                          |                    |
// -----------------|-----------------------------------|--------------------|
// JOB_LOCKDOWN     | Same as RESTRICTED                | *One active process|
//                  |                                   |  limit.            |
//                  |                                   | *Kill on Job close.|
//                  |                                   | *Kill on unhandled |
//                  |                                   |  exception.        |
//                  |                                   |                    |
// In the context of the above table, 'user handles' refers to the handles of
// windows, bitmaps, menus, etc. Files, treads and registry handles are kernel
// handles and are not affected by the job level settings.
enum JobLevel {
  JOB_LOCKDOWN = 0,
  JOB_RESTRICTED,
  JOB_LIMITED_USER,
  JOB_INTERACTIVE,
  JOB_UNPROTECTED,
  JOB_NONE
};

// These flags correspond to various process-level mitigations (eg. ASLR and
// DEP). Most are implemented via UpdateProcThreadAttribute() plus flags for
// the PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY attribute argument; documented
// here: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686880
// Some mitigations are implemented directly by the sandbox or emulated to
// the greatest extent possible when not directly supported by the OS.
// Flags that are unsupported for the target OS will be silently ignored.
// Flags that are invalid for their application (pre or post startup) will
// return SBOX_ERROR_BAD_PARAMS.
typedef uint64 MitigationFlags;

// Permanently enables DEP for the target process. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE.
const MitigationFlags MITIGATION_DEP                              = 0x00000001;

// Permanently Disables ATL thunk emulation when DEP is enabled. Valid
// only when MITIGATION_DEP is passed. Corresponds to not passing
// PROCESS_CREATION_MITIGATION_POLICY_DEP_ATL_THUNK_ENABLE.
const MitigationFlags MITIGATION_DEP_NO_ATL_THUNK                 = 0x00000002;

// Enables Structured exception handling override prevention. Must be
// enabled prior to process start. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE.
const MitigationFlags MITIGATION_SEHOP                            = 0x00000004;

// Forces ASLR on all images in the child process. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON .
const MitigationFlags MITIGATION_RELOCATE_IMAGE                   = 0x00000008;

// Refuses to load DLLs that cannot support ASLR. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON_REQ_RELOCS.
const MitigationFlags MITIGATION_RELOCATE_IMAGE_REQUIRED          = 0x00000010;

// Terminates the process on Windows heap corruption. Coresponds to
// PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON.
const MitigationFlags MITIGATION_HEAP_TERMINATE                   = 0x00000020;

// Sets a random lower bound as the minimum user address. Must be
// enabled prior to process start. On 32-bit processes this is
// emulated to a much smaller degree. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON.
const MitigationFlags MITIGATION_BOTTOM_UP_ASLR                   = 0x00000040;

// Increases the randomness range of bottom-up ASLR to up to 1TB. Must be
// enabled prior to process start and with MITIGATION_BOTTOM_UP_ASLR.
// Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON
const MitigationFlags MITIGATION_HIGH_ENTROPY_ASLR                = 0x00000080;

// Immediately raises an exception on a bad handle reference. Must be
// enabled after startup. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON.
const MitigationFlags MITIGATION_STRICT_HANDLE_CHECKS             = 0x00000100;

// Prevents the process from making Win32k calls. Must be enabled after
// startup. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON.
const MitigationFlags MITIGATION_WIN32K_DISABLE                   = 0x00000200;

// Disables common DLL injection methods (e.g. window hooks and
// App_InitDLLs). Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON.
const MitigationFlags MITIGATION_EXTENSION_DLL_DISABLE            = 0x00000400;

// Sets the DLL search order to LOAD_LIBRARY_SEARCH_DEFAULT_DIRS. Additional
// directories can be added via the Windows AddDllDirectory() function.
// http://msdn.microsoft.com/en-us/library/windows/desktop/hh310515
// Must be enabled after startup.
const MitigationFlags MITIGATION_DLL_SEARCH_ORDER        = 0x00000001ULL << 32;

}  // namespace sandbox

#endif  // SANDBOX_SRC_SECURITY_LEVEL_H_
