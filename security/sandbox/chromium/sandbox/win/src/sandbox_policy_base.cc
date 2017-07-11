// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_base.h"

#include <sddl.h>
#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/handle_policy.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/named_pipe_policy.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/src/process_thread_policy.h"
#include "sandbox/win/src/registry_policy.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/sync_policy.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/top_level_dispatcher.h"
#include "sandbox/win/src/window.h"

namespace {

// The standard windows size for one memory page.
const size_t kOneMemPage = 4096;
// The IPC and Policy shared memory sizes.
const size_t kIPCMemSize = kOneMemPage * 2;
const size_t kPolMemSize = kOneMemPage * 14;

// Helper function to allocate space (on the heap) for policy.
sandbox::PolicyGlobal* MakeBrokerPolicyMemory() {
  const size_t kTotalPolicySz = kPolMemSize;
  sandbox::PolicyGlobal* policy = static_cast<sandbox::PolicyGlobal*>
      (::operator new(kTotalPolicySz));
  DCHECK(policy);
  memset(policy, 0, kTotalPolicySz);
  policy->data_size = kTotalPolicySz - sizeof(sandbox::PolicyGlobal);
  return policy;
}

bool IsInheritableHandle(HANDLE handle) {
  if (!handle)
    return false;
  if (handle == INVALID_HANDLE_VALUE)
    return false;
  // File handles (FILE_TYPE_DISK) and pipe handles are known to be
  // inheritable.  Console handles (FILE_TYPE_CHAR) are not
  // inheritable via PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
  DWORD handle_type = GetFileType(handle);
  return handle_type == FILE_TYPE_DISK || handle_type == FILE_TYPE_PIPE;
}

HANDLE CreateLowBoxObjectDirectory(PSID lowbox_sid) {
  DWORD session_id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id))
    return NULL;

  LPWSTR sid_string = NULL;
  if (!::ConvertSidToStringSid(lowbox_sid, &sid_string))
    return NULL;

  base::string16 directory_path = base::StringPrintf(
                   L"\\Sessions\\%d\\AppContainerNamedObjects\\%ls",
                   session_id, sid_string).c_str();
  ::LocalFree(sid_string);

  NtCreateDirectoryObjectFunction CreateObjectDirectory = NULL;
  ResolveNTFunctionPtr("NtCreateDirectoryObject", &CreateObjectDirectory);

  OBJECT_ATTRIBUTES obj_attr;
  UNICODE_STRING obj_name;
  sandbox::InitObjectAttribs(directory_path,
                             OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
                             NULL,
                             &obj_attr,
                             &obj_name,
                             NULL);

  HANDLE handle = NULL;
  NTSTATUS status = CreateObjectDirectory(&handle,
                                          DIRECTORY_ALL_ACCESS,
                                          &obj_attr);

  if (!NT_SUCCESS(status))
    return NULL;

  return handle;
}

}  // namespace

namespace sandbox {

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level;
SANDBOX_INTERCEPT MitigationFlags g_shared_delayed_mitigations;

// Initializes static members.
HWINSTA PolicyBase::alternate_winstation_handle_ = NULL;
HDESK PolicyBase::alternate_desktop_handle_ = NULL;
IntegrityLevel PolicyBase::alternate_desktop_integrity_level_label_ =
    INTEGRITY_LEVEL_SYSTEM;

PolicyBase::PolicyBase()
    : ref_count(1),
      lockdown_level_(USER_LOCKDOWN),
      initial_level_(USER_LOCKDOWN),
      job_level_(JOB_LOCKDOWN),
      ui_exceptions_(0),
      memory_limit_(0),
      use_alternate_desktop_(false),
      use_alternate_winstation_(false),
      file_system_init_(false),
      relaxed_interceptions_(true),
      stdout_handle_(INVALID_HANDLE_VALUE),
      stderr_handle_(INVALID_HANDLE_VALUE),
      integrity_level_(INTEGRITY_LEVEL_LAST),
      delayed_integrity_level_(INTEGRITY_LEVEL_LAST),
      mitigations_(0),
      delayed_mitigations_(0),
      is_csrss_connected_(true),
      policy_maker_(NULL),
      policy_(NULL),
      lowbox_sid_(NULL),
      lockdown_default_dacl_(false),
      enable_opm_redirection_(false) {
  ::InitializeCriticalSection(&lock_);
  dispatcher_.reset(new TopLevelDispatcher(this));
}

PolicyBase::~PolicyBase() {
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    TargetProcess* target = (*it);
    delete target;
  }
  delete policy_maker_;
  delete policy_;

  if (lowbox_sid_)
    ::LocalFree(lowbox_sid_);

  ::DeleteCriticalSection(&lock_);
}

void PolicyBase::AddRef() {
  ::InterlockedIncrement(&ref_count);
}

void PolicyBase::Release() {
  if (0 == ::InterlockedDecrement(&ref_count))
    delete this;
}

ResultCode PolicyBase::SetTokenLevel(TokenLevel initial, TokenLevel lockdown) {
  if (initial < lockdown) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  initial_level_ = initial;
  lockdown_level_ = lockdown;
  return SBOX_ALL_OK;
}

TokenLevel PolicyBase::GetInitialTokenLevel() const {
  return initial_level_;
}

TokenLevel PolicyBase::GetLockdownTokenLevel() const {
  return lockdown_level_;
}

void PolicyBase::SetDoNotUseRestrictingSIDs() {
  use_restricting_sids_ = false;
}

ResultCode PolicyBase::SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) {
  if (memory_limit_ && job_level == JOB_NONE) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  job_level_ = job_level;
  ui_exceptions_ = ui_exceptions;
  return SBOX_ALL_OK;
}

JobLevel PolicyBase::GetJobLevel() const {
  return job_level_;
}

ResultCode PolicyBase::SetJobMemoryLimit(size_t memory_limit) {
  if (memory_limit && job_level_ == JOB_NONE) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  memory_limit_ = memory_limit;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetAlternateDesktop(bool alternate_winstation) {
  use_alternate_desktop_ = true;
  use_alternate_winstation_ = alternate_winstation;
  return CreateAlternateDesktop(alternate_winstation);
}

base::string16 PolicyBase::GetAlternateDesktop() const {
  // No alternate desktop or winstation. Return an empty string.
  if (!use_alternate_desktop_ && !use_alternate_winstation_) {
    return base::string16();
  }

  // The desktop and winstation should have been created by now.
  // If we hit this scenario, it means that the user ignored the failure
  // during SetAlternateDesktop, so we ignore it here too.
  if (use_alternate_desktop_ && !alternate_desktop_handle_) {
    return base::string16();
  }
  if (use_alternate_winstation_ && (!alternate_desktop_handle_ ||
                                    !alternate_winstation_handle_)) {
    return base::string16();
  }

  return GetFullDesktopName(alternate_winstation_handle_,
                            alternate_desktop_handle_);
}

ResultCode PolicyBase::CreateAlternateDesktop(bool alternate_winstation) {
  if (alternate_winstation) {
    // Previously called with alternate_winstation = false?
    if (!alternate_winstation_handle_ && alternate_desktop_handle_)
      return SBOX_ERROR_UNSUPPORTED;

    // Check if it's already created.
    if (alternate_winstation_handle_ && alternate_desktop_handle_)
      return SBOX_ALL_OK;

    DCHECK(!alternate_winstation_handle_);
    // Create the window station.
    ResultCode result = CreateAltWindowStation(&alternate_winstation_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_winstation_handle_ ||
        GetWindowObjectName(alternate_winstation_handle_).empty())
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;

    // Create the destkop.
    result = CreateAltDesktop(alternate_winstation_handle_,
                              &alternate_desktop_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_desktop_handle_ ||
        GetWindowObjectName(alternate_desktop_handle_).empty())
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
  } else {
    // Previously called with alternate_winstation = true?
    if (alternate_winstation_handle_)
      return SBOX_ERROR_UNSUPPORTED;

    // Check if it already exists.
    if (alternate_desktop_handle_)
      return SBOX_ALL_OK;

    // Create the destkop.
    ResultCode result = CreateAltDesktop(NULL, &alternate_desktop_handle_);
    if (SBOX_ALL_OK != result)
      return result;

    // Verify that everything is fine.
    if (!alternate_desktop_handle_ ||
        GetWindowObjectName(alternate_desktop_handle_).empty())
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
  }

  return SBOX_ALL_OK;
}

void PolicyBase::DestroyAlternateDesktop() {
  if (alternate_desktop_handle_) {
    ::CloseDesktop(alternate_desktop_handle_);
    alternate_desktop_handle_ = NULL;
  }

  if (alternate_winstation_handle_) {
    ::CloseWindowStation(alternate_winstation_handle_);
    alternate_winstation_handle_ = NULL;
  }
}

ResultCode PolicyBase::SetIntegrityLevel(IntegrityLevel integrity_level) {
  integrity_level_ = integrity_level;
  return SBOX_ALL_OK;
}

IntegrityLevel PolicyBase::GetIntegrityLevel() const {
  return integrity_level_;
}

ResultCode PolicyBase::SetDelayedIntegrityLevel(
    IntegrityLevel integrity_level) {
  delayed_integrity_level_ = integrity_level;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetCapability(const wchar_t* sid) {
  capabilities_.push_back(sid);
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetLowBox(const wchar_t* sid) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return SBOX_ERROR_UNSUPPORTED;

  DCHECK(sid);
  if (lowbox_sid_)
    return SBOX_ERROR_BAD_PARAMS;

  if (!ConvertStringSidToSid(sid, &lowbox_sid_))
    return SBOX_ERROR_GENERIC;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetProcessMitigations(
    MitigationFlags flags) {
  if (!CanSetProcessMitigationsPreStartup(flags))
    return SBOX_ERROR_BAD_PARAMS;
  mitigations_ = flags;
  return SBOX_ALL_OK;
}

MitigationFlags PolicyBase::GetProcessMitigations() {
  return mitigations_;
}

ResultCode PolicyBase::SetDelayedProcessMitigations(
    MitigationFlags flags) {
  if (!CanSetProcessMitigationsPostStartup(flags))
    return SBOX_ERROR_BAD_PARAMS;
  delayed_mitigations_ = flags;
  return SBOX_ALL_OK;
}

MitigationFlags PolicyBase::GetDelayedProcessMitigations() const {
  return delayed_mitigations_;
}

void PolicyBase::SetStrictInterceptions() {
  relaxed_interceptions_ = false;
}

ResultCode PolicyBase::SetStdoutHandle(HANDLE handle) {
  if (!IsInheritableHandle(handle))
    return SBOX_ERROR_BAD_PARAMS;
  stdout_handle_ = handle;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetStderrHandle(HANDLE handle) {
  if (!IsInheritableHandle(handle))
    return SBOX_ERROR_BAD_PARAMS;
  stderr_handle_ = handle;
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::AddRule(SubSystem subsystem,
                               Semantics semantics,
                               const wchar_t* pattern) {
  ResultCode result = AddRuleInternal(subsystem, semantics, pattern);
  LOG_IF(ERROR, result != SBOX_ALL_OK) << "Failed to add sandbox rule."
                                       << " error = " << result
                                       << ", subsystem = " << subsystem
                                       << ", semantics = " << semantics
                                       << ", pattern = '" << pattern << "'";
  return result;
}

ResultCode PolicyBase::AddDllToUnload(const wchar_t* dll_name) {
  blacklisted_dlls_.push_back(dll_name);
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::AddKernelObjectToClose(const base::char16* handle_type,
                                              const base::char16* handle_name) {
  return handle_closer_.AddHandle(handle_type, handle_name);
}

void PolicyBase::AddHandleToShare(HANDLE handle) {
  CHECK(handle && handle != INVALID_HANDLE_VALUE);

  // Ensure the handle can be inherited.
  BOOL result = SetHandleInformation(handle, HANDLE_FLAG_INHERIT,
                                     HANDLE_FLAG_INHERIT);
  PCHECK(result);

  handles_to_share_.push_back(handle);
}

void PolicyBase::SetLockdownDefaultDacl() {
  lockdown_default_dacl_ = true;
}

const base::HandlesToInheritVector& PolicyBase::GetHandlesBeingShared() {
  return handles_to_share_;
}

ResultCode PolicyBase::MakeJobObject(base::win::ScopedHandle* job) {
  if (job_level_ != JOB_NONE) {
    // Create the windows job object.
    Job job_obj;
    DWORD result = job_obj.Init(job_level_, NULL, ui_exceptions_,
                                memory_limit_);
    if (ERROR_SUCCESS != result)
      return SBOX_ERROR_GENERIC;

    *job = job_obj.Take();
  } else {
    *job = base::win::ScopedHandle();
  }
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::MakeTokens(base::win::ScopedHandle* initial,
                                  base::win::ScopedHandle* lockdown,
                                  base::win::ScopedHandle* lowbox) {
  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  DWORD result =
      CreateRestrictedToken(lockdown_level_, integrity_level_, PRIMARY,
                            lockdown_default_dacl_, use_restricting_sids_,
                            lockdown);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_GENERIC;

  // If we're launching on the alternate desktop we need to make sure the
  // integrity label on the object is no higher than the sandboxed process's
  // integrity level. So, we lower the label on the desktop process if it's
  // not already low enough for our process.
  if (alternate_desktop_handle_ && use_alternate_desktop_ &&
      integrity_level_ != INTEGRITY_LEVEL_LAST &&
      alternate_desktop_integrity_level_label_ < integrity_level_) {
    // Integrity label enum is reversed (higher level is a lower value).
    static_assert(INTEGRITY_LEVEL_SYSTEM < INTEGRITY_LEVEL_UNTRUSTED,
                  "Integrity level ordering reversed.");
    result = SetObjectIntegrityLabel(alternate_desktop_handle_,
                                     SE_WINDOW_OBJECT,
                                     L"",
                                     GetIntegrityLevelString(integrity_level_));
    if (ERROR_SUCCESS != result)
      return SBOX_ERROR_GENERIC;

    alternate_desktop_integrity_level_label_ = integrity_level_;
  }

  if (lowbox_sid_) {
    NtCreateLowBoxToken CreateLowBoxToken = NULL;
    ResolveNTFunctionPtr("NtCreateLowBoxToken", &CreateLowBoxToken);
    OBJECT_ATTRIBUTES obj_attr;
    InitializeObjectAttributes(&obj_attr, NULL, 0, NULL, NULL);
    HANDLE token_lowbox = NULL;

    if (!lowbox_directory_.IsValid())
      lowbox_directory_.Set(CreateLowBoxObjectDirectory(lowbox_sid_));
    DCHECK(lowbox_directory_.IsValid());

    // The order of handles isn't important in the CreateLowBoxToken call.
    // The kernel will maintain a reference to the object directory handle.
    HANDLE saved_handles[1] = {lowbox_directory_.Get()};
    DWORD saved_handles_count = lowbox_directory_.IsValid() ? 1 : 0;

    NTSTATUS status = CreateLowBoxToken(&token_lowbox, lockdown->Get(),
                                        TOKEN_ALL_ACCESS, &obj_attr,
                                        lowbox_sid_, 0, NULL,
                                        saved_handles_count, saved_handles);
    if (!NT_SUCCESS(status))
      return SBOX_ERROR_GENERIC;

    DCHECK(token_lowbox);
    lowbox->Set(token_lowbox);
  }

  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  result =
      CreateRestrictedToken(initial_level_, integrity_level_, IMPERSONATION,
                            lockdown_default_dacl_, use_restricting_sids_,
                            initial);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_GENERIC;

  return SBOX_ALL_OK;
}

PSID PolicyBase::GetLowBoxSid() const {
  return lowbox_sid_;
}

ResultCode PolicyBase::AddTarget(TargetProcess* target) {
  if (NULL != policy_)
    policy_maker_->Done();

  if (!ApplyProcessMitigationsToSuspendedProcess(target->Process(),
                                                 mitigations_)) {
    return SBOX_ERROR_APPLY_ASLR_MITIGATIONS;
  }

  ResultCode ret = SetupAllInterceptions(target);

  if (ret != SBOX_ALL_OK)
    return ret;

  if (!SetupHandleCloser(target))
    return SBOX_ERROR_SETUP_HANDLE_CLOSER;

  DWORD win_error = ERROR_SUCCESS;
  // Initialize the sandbox infrastructure for the target.
  // TODO(wfh) do something with win_error code here.
  ret = target->Init(dispatcher_.get(), policy_, kIPCMemSize, kPolMemSize,
                     &win_error);

  if (ret != SBOX_ALL_OK)
    return ret;

  g_shared_delayed_integrity_level = delayed_integrity_level_;
  ret = target->TransferVariable("g_shared_delayed_integrity_level",
                                 &g_shared_delayed_integrity_level,
                                 sizeof(g_shared_delayed_integrity_level));
  g_shared_delayed_integrity_level = INTEGRITY_LEVEL_LAST;
  if (SBOX_ALL_OK != ret)
    return ret;

  // Add in delayed mitigations and pseudo-mitigations enforced at startup.
  g_shared_delayed_mitigations = delayed_mitigations_ |
      FilterPostStartupProcessMitigations(mitigations_);
  if (!CanSetProcessMitigationsPostStartup(g_shared_delayed_mitigations))
    return SBOX_ERROR_BAD_PARAMS;

  ret = target->TransferVariable("g_shared_delayed_mitigations",
                                 &g_shared_delayed_mitigations,
                                 sizeof(g_shared_delayed_mitigations));
  g_shared_delayed_mitigations = 0;
  if (SBOX_ALL_OK != ret)
    return ret;

  AutoLock lock(&lock_);
  targets_.push_back(target);
  return SBOX_ALL_OK;
}

bool PolicyBase::OnJobEmpty(HANDLE job) {
  AutoLock lock(&lock_);
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    if ((*it)->Job() == job)
      break;
  }
  if (it == targets_.end()) {
    return false;
  }
  TargetProcess* target = *it;
  targets_.erase(it);
  delete target;
  return true;
}

void PolicyBase::SetDisconnectCsrss() {
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    is_csrss_connected_ = false;
    AddKernelObjectToClose(L"ALPC Port", NULL);
  }
}

EvalResult PolicyBase::EvalPolicy(int service,
                                  CountedParameterSetBase* params) {
  if (NULL != policy_) {
    if (NULL == policy_->entry[service]) {
      // There is no policy for this particular service. This is not a big
      // deal.
      return DENY_ACCESS;
    }
    for (int i = 0; i < params->count; i++) {
      if (!params->parameters[i].IsValid()) {
        NOTREACHED();
        return SIGNAL_ALARM;
      }
    }
    PolicyProcessor pol_evaluator(policy_->entry[service]);
    PolicyResult result =  pol_evaluator.Evaluate(kShortEval,
                                                  params->parameters,
                                                  params->count);
    if (POLICY_MATCH == result) {
      return pol_evaluator.GetAction();
    }
    DCHECK(POLICY_ERROR != result);
  }

  return DENY_ACCESS;
}

HANDLE PolicyBase::GetStdoutHandle() {
  return stdout_handle_;
}

HANDLE PolicyBase::GetStderrHandle() {
  return stderr_handle_;
}

void PolicyBase::SetEnableOPMRedirection() {
  enable_opm_redirection_ = true;
}

bool PolicyBase::GetEnableOPMRedirection() {
  return enable_opm_redirection_;
}

ResultCode PolicyBase::SetupAllInterceptions(TargetProcess* target) {
  InterceptionManager manager(target, relaxed_interceptions_);

  if (policy_) {
    for (int i = 0; i < IPC_LAST_TAG; i++) {
      if (policy_->entry[i] && !dispatcher_->SetupService(&manager, i))
        return SBOX_ERROR_SETUP_INTERCEPTION_SERVICE;
    }
  }

  if (!blacklisted_dlls_.empty()) {
    std::vector<base::string16>::iterator it = blacklisted_dlls_.begin();
    for (; it != blacklisted_dlls_.end(); ++it) {
      manager.AddToUnloadModules(it->c_str());
    }
  }

  if (!SetupBasicInterceptions(&manager, is_csrss_connected_))
    return SBOX_ERROR_SETUP_BASIC_INTERCEPTIONS;

  ResultCode rc = manager.InitializeInterceptions();
  if (rc != SBOX_ALL_OK)
    return rc;

  // Finally, setup imports on the target so the interceptions can work.
  if (!SetupNtdllImports(target))
    return SBOX_ERROR_SETUP_NTDLL_IMPORTS;

  return SBOX_ALL_OK;
}

bool PolicyBase::SetupHandleCloser(TargetProcess* target) {
  return handle_closer_.InitializeTargetHandles(target);
}

ResultCode PolicyBase::AddRuleInternal(SubSystem subsystem,
                                       Semantics semantics,
                                       const wchar_t* pattern) {
  if (NULL == policy_) {
    policy_ = MakeBrokerPolicyMemory();
    DCHECK(policy_);
    policy_maker_ = new LowLevelPolicy(policy_);
    DCHECK(policy_maker_);
  }

  switch (subsystem) {
    case SUBSYS_FILES: {
      if (!file_system_init_) {
        if (!FileSystemPolicy::SetInitialRules(policy_maker_))
          return SBOX_ERROR_BAD_PARAMS;
        file_system_init_ = true;
      }
      if (!FileSystemPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_SYNC: {
      if (!SyncPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_PROCESS: {
      if (lockdown_level_ < USER_INTERACTIVE &&
          TargetPolicy::PROCESS_ALL_EXEC == semantics) {
        // This is unsupported. This is a huge security risk to give full access
        // to a process handle.
        return SBOX_ERROR_UNSUPPORTED;
      }
      if (!ProcessPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_NAMED_PIPES: {
      if (!NamedPipePolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_REGISTRY: {
      if (!RegistryPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_HANDLES: {
      if (!HandlePolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }

    case SUBSYS_WIN32K_LOCKDOWN: {
      if (!ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
              pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }

    default: { return SBOX_ERROR_UNSUPPORTED; }
  }

  return SBOX_ALL_OK;
}

}  // namespace sandbox
