// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_H_

#include <string>

#include "base/basictypes.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"

namespace sandbox {

class TargetPolicy {
 public:
  // Windows subsystems that can have specific rules.
  // Note: The process subsystem(SUBSY_PROCESS) does not evaluate the request
  // exactly like the CreateProcess API does. See the comment at the top of
  // process_thread_dispatcher.cc for more details.
  enum SubSystem {
    SUBSYS_FILES,             // Creation and opening of files and pipes.
    SUBSYS_NAMED_PIPES,       // Creation of named pipes.
    SUBSYS_PROCESS,           // Creation of child processes.
    SUBSYS_REGISTRY,          // Creation and opening of registry keys.
    SUBSYS_SYNC,              // Creation of named sync objects.
    SUBSYS_HANDLES            // Duplication of handles to other processes.
  };

  // Allowable semantics when a rule is matched.
  enum Semantics {
    FILES_ALLOW_ANY,       // Allows open or create for any kind of access that
                           // the file system supports.
    FILES_ALLOW_READONLY,  // Allows open or create with read access only.
    FILES_ALLOW_QUERY,     // Allows access to query the attributes of a file.
    FILES_ALLOW_DIR_ANY,   // Allows open or create with directory semantics
                           // only.
    HANDLES_DUP_ANY,       // Allows duplicating handles opened with any
                           // access permissions.
    HANDLES_DUP_BROKER,    // Allows duplicating handles to the broker process.
    NAMEDPIPES_ALLOW_ANY,  // Allows creation of a named pipe.
    PROCESS_MIN_EXEC,      // Allows to create a process with minimal rights
                           // over the resulting process and thread handles.
                           // No other parameters besides the command line are
                           // passed to the child process.
    PROCESS_ALL_EXEC,      // Allows the creation of a process and return fill
                           // access on the returned handles.
                           // This flag can be used only when the main token of
                           // the sandboxed application is at least INTERACTIVE.
    EVENTS_ALLOW_ANY,      // Allows the creation of an event with full access.
    EVENTS_ALLOW_READONLY, // Allows opening an even with synchronize access.
    REG_ALLOW_READONLY,    // Allows readonly access to a registry key.
    REG_ALLOW_ANY          // Allows read and write access to a registry key.
  };

  // Increments the reference count of this object. The reference count must
  // be incremented if this interface is given to another component.
  virtual void AddRef() = 0;

  // Decrements the reference count of this object. When the reference count
  // is zero the object is automatically destroyed.
  // Indicates that the caller is done with this interface. After calling
  // release no other method should be called.
  virtual void Release() = 0;

  // Sets the security level for the target process' two tokens.
  // This setting is permanent and cannot be changed once the target process is
  // spawned.
  // initial: the security level for the initial token. This is the token that
  //   is used by the process from the creation of the process until the moment
  //   the process calls TargetServices::LowerToken() or the process calls
  //   win32's ReverToSelf(). Once this happens the initial token is no longer
  //   available and the lockdown token is in effect. Using an initial token is
  //   not compatible with AppContainer, see SetAppContainer.
  // lockdown: the security level for the token that comes into force after the
  //   process calls TargetServices::LowerToken() or the process calls
  //   ReverToSelf(). See the explanation of each level in the TokenLevel
  //   definition.
  // Return value: SBOX_ALL_OK if the setting succeeds and false otherwise.
  //   Returns false if the lockdown value is more permissive than the initial
  //   value.
  //
  // Important: most of the sandbox-provided security relies on this single
  // setting. The caller should strive to set the lockdown level as restricted
  // as possible.
  virtual ResultCode SetTokenLevel(TokenLevel initial, TokenLevel lockdown) = 0;

  // Sets the security level of the Job Object to which the target process will
  // belong. This setting is permanent and cannot be changed once the target
  // process is spawned. The job controls the global security settings which
  // can not be specified in the token security profile.
  // job_level: the security level for the job. See the explanation of each
  //   level in the JobLevel definition.
  // ui_exceptions: specify what specific rights that are disabled in the
  //   chosen job_level that need to be granted. Use this parameter to avoid
  //   selecting the next permissive job level unless you need all the rights
  //   that are granted in such level.
  //   The exceptions can be specified as a combination of the following
  //   constants:
  // JOB_OBJECT_UILIMIT_HANDLES : grant access to all user-mode handles. These
  //   include windows, icons, menus and various GDI objects. In addition the
  //   target process can set hooks, and broadcast messages to other processes
  //   that belong to the same desktop.
  // JOB_OBJECT_UILIMIT_READCLIPBOARD : grant read-only access to the clipboard.
  // JOB_OBJECT_UILIMIT_WRITECLIPBOARD : grant write access to the clipboard.
  // JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS : allow changes to the system-wide
  //   parameters as defined by the Win32 call SystemParametersInfo().
  // JOB_OBJECT_UILIMIT_DISPLAYSETTINGS : allow programmatic changes to the
  //  display settings.
  // JOB_OBJECT_UILIMIT_GLOBALATOMS : allow access to the global atoms table.
  // JOB_OBJECT_UILIMIT_DESKTOP : allow the creation of new desktops.
  // JOB_OBJECT_UILIMIT_EXITWINDOWS : allow the call to ExitWindows().
  //
  // Return value: SBOX_ALL_OK if the setting succeeds and false otherwise.
  //
  // Note: JOB_OBJECT_XXXX constants are defined in winnt.h and documented at
  // length in:
  //   http://msdn2.microsoft.com/en-us/library/ms684152.aspx
  //
  // Note: the recommended level is JOB_RESTRICTED or JOB_LOCKDOWN.
  virtual ResultCode SetJobLevel(JobLevel job_level, uint32 ui_exceptions) = 0;

  // Specifies the desktop on which the application is going to run. If the
  // desktop does not exist, it will be created. If alternate_winstation is
  // set to true, the desktop will be created on an alternate window station.
  virtual ResultCode SetAlternateDesktop(bool alternate_winstation) = 0;

  // Returns the name of the alternate desktop used. If an alternate window
  // station is specified, the name is prepended by the window station name,
  // followed by a backslash.
  virtual std::wstring GetAlternateDesktop() const = 0;

  // Precreates the desktop and window station, if any.
  virtual ResultCode CreateAlternateDesktop(bool alternate_winstation) = 0;

  // Destroys the desktop and windows station.
  virtual void DestroyAlternateDesktop() = 0;

  // Sets the integrity level of the process in the sandbox. Both the initial
  // token and the main token will be affected by this. If the integrity level
  // is set to a level higher than the current level, the sandbox will fail
  // to start.
  virtual ResultCode SetIntegrityLevel(IntegrityLevel level) = 0;

  // Sets the integrity level of the process in the sandbox. The integrity level
  // will not take effect before you call LowerToken. User Interface Privilege
  // Isolation is not affected by this setting and will remain off for the
  // process in the sandbox. If the integrity level is set to a level higher
  // than the current level, the sandbox will fail to start.
  virtual ResultCode SetDelayedIntegrityLevel(IntegrityLevel level) = 0;

  // Sets the AppContainer to be used for the sandboxed process. Any capability
  // to be enabled for the process should be added before this method is invoked
  // (by calling SetCapability() as many times as needed).
  // The desired AppContainer must be already installed on the system, otherwise
  // launching the sandboxed process will fail. See BrokerServices for details
  // about installing an AppContainer.
  // Note that currently Windows restricts the use of impersonation within
  // AppContainers, so this function is incompatible with the use of an initial
  // token.
  virtual ResultCode SetAppContainer(const wchar_t* sid) = 0;

  // Sets a capability to be enabled for the sandboxed process' AppContainer.
  virtual ResultCode SetCapability(const wchar_t* sid) = 0;

  // Sets the mitigations enabled when the process is created. Most of these
  // are implemented as attributes passed via STARTUPINFOEX. So they take
  // effect before any thread in the target executes. The declaration of
  // MitigationFlags is followed by a detailed description of each flag.
  virtual ResultCode SetProcessMitigations(MitigationFlags flags) = 0;

  // Returns the currently set mitigation flags.
  virtual MitigationFlags GetProcessMitigations() = 0;

  // Sets process mitigation flags that don't take effect before the call to
  // LowerToken().
  virtual ResultCode SetDelayedProcessMitigations(MitigationFlags flags) = 0;

  // Returns the currently set delayed mitigation flags.
  virtual MitigationFlags GetDelayedProcessMitigations() = 0;

  // Sets the interceptions to operate in strict mode. By default, interceptions
  // are performed in "relaxed" mode, where if something inside NTDLL.DLL is
  // already patched we attempt to intercept it anyway. Setting interceptions
  // to strict mode means that when we detect that the function is patched we'll
  // refuse to perform the interception.
  virtual void SetStrictInterceptions() = 0;

  // Set the handles the target process should inherit for stdout and
  // stderr.  The handles the caller passes must remain valid for the
  // lifetime of the policy object.  This only has an effect on
  // Windows Vista and later versions.  These methods accept pipe and
  // file handles, but not console handles.
  virtual ResultCode SetStdoutHandle(HANDLE handle) = 0;
  virtual ResultCode SetStderrHandle(HANDLE handle) = 0;

  // Adds a policy rule effective for processes spawned using this policy.
  // subsystem: One of the above enumerated windows subsystems.
  // semantics: One of the above enumerated FileSemantics.
  // pattern: A specific full path or a full path with wildcard patterns.
  //   The valid wildcards are:
  //   '*' : Matches zero or more character. Only one in series allowed.
  //   '?' : Matches a single character. One or more in series are allowed.
  // Examples:
  //   "c:\\documents and settings\\vince\\*.dmp"
  //   "c:\\documents and settings\\*\\crashdumps\\*.dmp"
  //   "c:\\temp\\app_log_?????_chrome.txt"
  virtual ResultCode AddRule(SubSystem subsystem, Semantics semantics,
                             const wchar_t* pattern) = 0;

  // Adds a dll that will be unloaded in the target process before it gets
  // a chance to initialize itself. Typically, dlls that cause the target
  // to crash go here.
  virtual ResultCode AddDllToUnload(const wchar_t* dll_name) = 0;

  // Adds a handle that will be closed in the target process after lockdown.
  // A NULL value for handle_name indicates all handles of the specified type.
  // An empty string for handle_name indicates the handle is unnamed.
  virtual ResultCode AddKernelObjectToClose(const wchar_t* handle_type,
                                            const wchar_t* handle_name) = 0;
};

}  // namespace sandbox


#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_H_
