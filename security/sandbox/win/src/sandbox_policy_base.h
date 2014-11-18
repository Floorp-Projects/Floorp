// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_

#include <windows.h>

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

class AppContainerAttributes;
class LowLevelPolicy;
class TargetProcess;
struct PolicyGlobal;

// We act as a policy dispatcher, implementing the handler for the "ping" IPC,
// so we have to provide the appropriate handler on the OnMessageReady method.
// There is a static_cast for the handler, and the compiler only performs the
// cast if the first base class is Dispatcher.
class PolicyBase : public Dispatcher, public TargetPolicy {
 public:
  PolicyBase();

  // TargetPolicy:
  virtual void AddRef() OVERRIDE;
  virtual void Release() OVERRIDE;
  virtual ResultCode SetTokenLevel(TokenLevel initial,
                                   TokenLevel lockdown) OVERRIDE;
  virtual TokenLevel GetInitialTokenLevel() const OVERRIDE;
  virtual TokenLevel GetLockdownTokenLevel() const OVERRIDE;
  virtual ResultCode SetJobLevel(JobLevel job_level,
                                 uint32 ui_exceptions) OVERRIDE;
  virtual ResultCode SetJobMemoryLimit(size_t memory_limit) OVERRIDE;
  virtual ResultCode SetAlternateDesktop(bool alternate_winstation) OVERRIDE;
  virtual base::string16 GetAlternateDesktop() const OVERRIDE;
  virtual ResultCode CreateAlternateDesktop(bool alternate_winstation) OVERRIDE;
  virtual void DestroyAlternateDesktop() OVERRIDE;
  virtual ResultCode SetIntegrityLevel(IntegrityLevel integrity_level) OVERRIDE;
  virtual IntegrityLevel GetIntegrityLevel() const OVERRIDE;
  virtual ResultCode SetDelayedIntegrityLevel(
      IntegrityLevel integrity_level) OVERRIDE;
  virtual ResultCode SetAppContainer(const wchar_t* sid) OVERRIDE;
  virtual ResultCode SetCapability(const wchar_t* sid) OVERRIDE;
  virtual ResultCode SetProcessMitigations(MitigationFlags flags) OVERRIDE;
  virtual MitigationFlags GetProcessMitigations() OVERRIDE;
  virtual ResultCode SetDelayedProcessMitigations(
      MitigationFlags flags) OVERRIDE;
  virtual MitigationFlags GetDelayedProcessMitigations() const OVERRIDE;
  virtual void SetStrictInterceptions() OVERRIDE;
  virtual ResultCode SetStdoutHandle(HANDLE handle) OVERRIDE;
  virtual ResultCode SetStderrHandle(HANDLE handle) OVERRIDE;
  virtual ResultCode AddRule(SubSystem subsystem, Semantics semantics,
                             const wchar_t* pattern) OVERRIDE;
  virtual ResultCode AddDllToUnload(const wchar_t* dll_name);
  virtual ResultCode AddKernelObjectToClose(
      const base::char16* handle_type,
      const base::char16* handle_name) OVERRIDE;

  // Dispatcher:
  virtual Dispatcher* OnMessageReady(IPCParams* ipc,
                                     CallbackGeneric* callback) OVERRIDE;
  virtual bool SetupService(InterceptionManager* manager, int service) OVERRIDE;

  // Creates a Job object with the level specified in a previous call to
  // SetJobLevel().
  ResultCode MakeJobObject(HANDLE* job);

  // Creates the two tokens with the levels specified in a previous call to
  // SetTokenLevel().
  ResultCode MakeTokens(HANDLE* initial, HANDLE* lockdown);

  const AppContainerAttributes* GetAppContainer();

  // Adds a target process to the internal list of targets. Internally a
  // call to TargetProcess::Init() is issued.
  bool AddTarget(TargetProcess* target);

  // Called when there are no more active processes in a Job.
  // Removes a Job object associated with this policy and the target associated
  // with the job.
  bool OnJobEmpty(HANDLE job);

  EvalResult EvalPolicy(int service, CountedParameterSetBase* params);

  HANDLE GetStdoutHandle();
  HANDLE GetStderrHandle();

 private:
  ~PolicyBase();

  // Test IPC providers.
  bool Ping(IPCInfo* ipc, void* cookie);

  // Returns a dispatcher from ipc_targets_.
  Dispatcher* GetDispatcher(int ipc_tag);

  // Sets up interceptions for a new target.
  bool SetupAllInterceptions(TargetProcess* target);

  // Sets up the handle closer for a new target.
  bool SetupHandleCloser(TargetProcess* target);

  // This lock synchronizes operations on the targets_ collection.
  CRITICAL_SECTION lock_;
  // Maintains the list of target process associated with this policy.
  // The policy takes ownership of them.
  typedef std::list<TargetProcess*> TargetSet;
  TargetSet targets_;
  // Standard object-lifetime reference counter.
  volatile LONG ref_count;
  // The user-defined global policy settings.
  TokenLevel lockdown_level_;
  TokenLevel initial_level_;
  JobLevel job_level_;
  uint32 ui_exceptions_;
  size_t memory_limit_;
  bool use_alternate_desktop_;
  bool use_alternate_winstation_;
  // Helps the file system policy initialization.
  bool file_system_init_;
  bool relaxed_interceptions_;
  HANDLE stdout_handle_;
  HANDLE stderr_handle_;
  IntegrityLevel integrity_level_;
  IntegrityLevel delayed_integrity_level_;
  MitigationFlags mitigations_;
  MitigationFlags delayed_mitigations_;
  // The array of objects that will answer IPC calls.
  Dispatcher* ipc_targets_[IPC_LAST_TAG];
  // Object in charge of generating the low level policy.
  LowLevelPolicy* policy_maker_;
  // Memory structure that stores the low level policy.
  PolicyGlobal* policy_;
  // The list of dlls to unload in the target process.
  std::vector<base::string16> blacklisted_dlls_;
  // This is a map of handle-types to names that we need to close in the
  // target process. A null set means we need to close all handles of the
  // given type.
  HandleCloser handle_closer_;
  std::vector<base::string16> capabilities_;
  scoped_ptr<AppContainerAttributes> appcontainer_list_;

  static HDESK alternate_desktop_handle_;
  static HWINSTA alternate_winstation_handle_;
  static IntegrityLevel alternate_desktop_integrity_level_label_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBase);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
