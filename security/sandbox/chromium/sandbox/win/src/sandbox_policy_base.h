// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
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

typedef std::vector<base::win::ScopedHandle*> HandleList;

class PolicyBase final : public TargetPolicy {
 public:
  PolicyBase();

  // TargetPolicy:
  void AddRef() override;
  void Release() override;
  ResultCode SetTokenLevel(TokenLevel initial, TokenLevel lockdown) override;
  TokenLevel GetInitialTokenLevel() const override;
  TokenLevel GetLockdownTokenLevel() const override;
  ResultCode SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) override;
  ResultCode SetJobMemoryLimit(size_t memory_limit) override;
  ResultCode SetAlternateDesktop(bool alternate_winstation) override;
  base::string16 GetAlternateDesktop() const override;
  ResultCode CreateAlternateDesktop(bool alternate_winstation) override;
  void DestroyAlternateDesktop() override;
  ResultCode SetIntegrityLevel(IntegrityLevel integrity_level) override;
  IntegrityLevel GetIntegrityLevel() const override;
  ResultCode SetDelayedIntegrityLevel(IntegrityLevel integrity_level) override;
  ResultCode SetAppContainer(const wchar_t* sid) override;
  ResultCode SetCapability(const wchar_t* sid) override;
  ResultCode SetLowBox(const wchar_t* sid) override;
  ResultCode SetProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetProcessMitigations() override;
  ResultCode SetDelayedProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetDelayedProcessMitigations() const override;
  void SetStrictInterceptions() override;
  ResultCode SetStdoutHandle(HANDLE handle) override;
  ResultCode SetStderrHandle(HANDLE handle) override;
  ResultCode AddRule(SubSystem subsystem,
                     Semantics semantics,
                     const wchar_t* pattern) override;
  ResultCode AddDllToUnload(const wchar_t* dll_name) override;
  ResultCode AddKernelObjectToClose(const base::char16* handle_type,
                                    const base::char16* handle_name) override;
  void* AddHandleToShare(HANDLE handle) override;

  // Creates a Job object with the level specified in a previous call to
  // SetJobLevel().
  ResultCode MakeJobObject(base::win::ScopedHandle* job);

  // Creates the two tokens with the levels specified in a previous call to
  // SetTokenLevel(). Also creates a lowbox token if specified based on the
  // lowbox SID.
  ResultCode MakeTokens(base::win::ScopedHandle* initial,
                        base::win::ScopedHandle* lockdown,
                        base::win::ScopedHandle* lowbox);

  const AppContainerAttributes* GetAppContainer() const;

  PSID GetLowBoxSid() const;

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

  // Returns the list of handles being shared with the target process.
  const HandleList& GetHandlesBeingShared();

  // Closes the handles being shared with the target and clears out the list.
  void ClearSharedHandles();

 private:
  ~PolicyBase();

  // Sets up interceptions for a new target.
  bool SetupAllInterceptions(TargetProcess* target);

  // Sets up the handle closer for a new target.
  bool SetupHandleCloser(TargetProcess* target);

  ResultCode AddRuleInternal(SubSystem subsystem,
                             Semantics semantics,
                             const wchar_t* pattern);

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
  uint32_t ui_exceptions_;
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
  PSID lowbox_sid_;
  base::win::ScopedHandle lowbox_directory_;
  scoped_ptr<Dispatcher> dispatcher_;

  static HDESK alternate_desktop_handle_;
  static HWINSTA alternate_winstation_handle_;
  static IntegrityLevel alternate_desktop_integrity_level_label_;

  // Contains the list of handles being shared with the target process.
  // This list contains handles other than the stderr/stdout handles which are
  // shared with the target at times.
  HandleList handles_to_share_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBase);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
