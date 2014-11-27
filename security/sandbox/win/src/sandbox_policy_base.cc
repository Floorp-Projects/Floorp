// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_base.h"

#include <sddl.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/filesystem_dispatcher.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/handle_dispatcher.h"
#include "sandbox/win/src/handle_policy.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/named_pipe_dispatcher.h"
#include "sandbox/win/src/named_pipe_policy.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/src/process_thread_dispatcher.h"
#include "sandbox/win/src/process_thread_policy.h"
#include "sandbox/win/src/registry_dispatcher.h"
#include "sandbox/win/src/registry_policy.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sync_dispatcher.h"
#include "sandbox/win/src/sync_policy.h"
#include "sandbox/win/src/target_process.h"
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

}

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
      policy_maker_(NULL),
      policy_(NULL) {
  ::InitializeCriticalSection(&lock_);
  // Initialize the IPC dispatcher array.
  memset(&ipc_targets_, NULL, sizeof(ipc_targets_));
  Dispatcher* dispatcher = NULL;

  dispatcher = new FilesystemDispatcher(this);
  ipc_targets_[IPC_NTCREATEFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTSETINFO_RENAME_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYATTRIBUTESFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYFULLATTRIBUTESFILE_TAG] = dispatcher;

  dispatcher = new NamedPipeDispatcher(this);
  ipc_targets_[IPC_CREATENAMEDPIPEW_TAG] = dispatcher;

  dispatcher = new ThreadProcessDispatcher(this);
  ipc_targets_[IPC_NTOPENTHREAD_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESS_TAG] = dispatcher;
  ipc_targets_[IPC_CREATEPROCESSW_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKEN_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKENEX_TAG] = dispatcher;

  dispatcher = new SyncDispatcher(this);
  ipc_targets_[IPC_CREATEEVENT_TAG] = dispatcher;
  ipc_targets_[IPC_OPENEVENT_TAG] = dispatcher;

  dispatcher = new RegistryDispatcher(this);
  ipc_targets_[IPC_NTCREATEKEY_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENKEY_TAG] = dispatcher;

  dispatcher = new HandleDispatcher(this);
  ipc_targets_[IPC_DUPLICATEHANDLEPROXY_TAG] = dispatcher;

  dispatcher = new ProcessMitigationsWin32KDispatcher(this);
  ipc_targets_[IPC_GDI_GDIDLLINITIALIZE_TAG] = dispatcher;
  ipc_targets_[IPC_GDI_GETSTOCKOBJECT_TAG] = dispatcher;
  ipc_targets_[IPC_USER_REGISTERCLASSW_TAG] = dispatcher;
}

PolicyBase::~PolicyBase() {
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    TargetProcess* target = (*it);
    delete target;
  }
  delete ipc_targets_[IPC_NTCREATEFILE_TAG];
  delete ipc_targets_[IPC_CREATENAMEDPIPEW_TAG];
  delete ipc_targets_[IPC_NTOPENTHREAD_TAG];
  delete ipc_targets_[IPC_CREATEEVENT_TAG];
  delete ipc_targets_[IPC_NTCREATEKEY_TAG];
  delete ipc_targets_[IPC_DUPLICATEHANDLEPROXY_TAG];
  delete policy_maker_;
  delete policy_;
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

TokenLevel PolicyBase::GetLockdownTokenLevel() const{
  return lockdown_level_;
}

ResultCode PolicyBase::SetJobLevel(JobLevel job_level, uint32 ui_exceptions) {
  if (memory_limit_ && job_level == JOB_NONE) {
    return SBOX_ERROR_BAD_PARAMS;
  }
  job_level_ = job_level;
  ui_exceptions_ = ui_exceptions;
  return SBOX_ALL_OK;
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

ResultCode PolicyBase::SetAppContainer(const wchar_t* sid) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return SBOX_ALL_OK;

  // Windows refuses to work with an impersonation token for a process inside
  // an AppContainer. If the caller wants to use a more privileged initial
  // token, or if the lockdown level will prevent the process from starting,
  // we have to fail the operation.
  if (lockdown_level_ < USER_LIMITED || lockdown_level_ != initial_level_)
    return SBOX_ERROR_CANNOT_INIT_APPCONTAINER;

  DCHECK(!appcontainer_list_.get());
  appcontainer_list_.reset(new AppContainerAttributes);
  ResultCode rv = appcontainer_list_->SetAppContainer(sid, capabilities_);
  if (rv != SBOX_ALL_OK)
    return rv;

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::SetCapability(const wchar_t* sid) {
  capabilities_.push_back(sid);
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

ResultCode PolicyBase::AddRule(SubSystem subsystem, Semantics semantics,
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
      if (lockdown_level_  < USER_INTERACTIVE &&
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
              pattern, semantics,policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }

    default: {
      return SBOX_ERROR_UNSUPPORTED;
    }
  }

  return SBOX_ALL_OK;
}

ResultCode PolicyBase::AddDllToUnload(const wchar_t* dll_name) {
  blacklisted_dlls_.push_back(dll_name);
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::AddKernelObjectToClose(const base::char16* handle_type,
                                              const base::char16* handle_name) {
  return handle_closer_.AddHandle(handle_type, handle_name);
}

// When an IPC is ready in any of the targets we get called. We manage an array
// of IPC dispatchers which are keyed on the IPC tag so we normally delegate
// to the appropriate dispatcher unless we can handle the IPC call ourselves.
Dispatcher* PolicyBase::OnMessageReady(IPCParams* ipc,
                                       CallbackGeneric* callback) {
  DCHECK(callback);
  static const IPCParams ping1 = {IPC_PING1_TAG, ULONG_TYPE};
  static const IPCParams ping2 = {IPC_PING2_TAG, INOUTPTR_TYPE};

  if (ping1.Matches(ipc) || ping2.Matches(ipc)) {
    *callback = reinterpret_cast<CallbackGeneric>(
                    static_cast<Callback1>(&PolicyBase::Ping));
    return this;
  }

  Dispatcher* dispatch = GetDispatcher(ipc->ipc_tag);
  if (!dispatch) {
    NOTREACHED();
    return NULL;
  }
  return dispatch->OnMessageReady(ipc, callback);
}

// Delegate to the appropriate dispatcher.
bool PolicyBase::SetupService(InterceptionManager* manager, int service) {
  if (IPC_PING1_TAG == service || IPC_PING2_TAG == service)
    return true;

  Dispatcher* dispatch = GetDispatcher(service);
  if (!dispatch) {
    NOTREACHED();
    return false;
  }
  return dispatch->SetupService(manager, service);
}

ResultCode PolicyBase::MakeJobObject(HANDLE* job) {
  if (job_level_ != JOB_NONE) {
    // Create the windows job object.
    Job job_obj;
    DWORD result = job_obj.Init(job_level_, NULL, ui_exceptions_,
                                memory_limit_);
    if (ERROR_SUCCESS != result) {
      return SBOX_ERROR_GENERIC;
    }
    *job = job_obj.Detach();
  } else {
    *job = NULL;
  }
  return SBOX_ALL_OK;
}

ResultCode PolicyBase::MakeTokens(HANDLE* initial, HANDLE* lockdown) {
  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  DWORD result = CreateRestrictedToken(lockdown, lockdown_level_,
                                       integrity_level_, PRIMARY);
  if (ERROR_SUCCESS != result)
    return SBOX_ERROR_GENERIC;

  // If we're launching on the alternate desktop we need to make sure the
  // integrity label on the object is no higher than the sandboxed process's
  // integrity level. So, we lower the label on the desktop process if it's
  // not already low enough for our process.
  if (use_alternate_desktop_ &&
      integrity_level_ != INTEGRITY_LEVEL_LAST &&
      alternate_desktop_integrity_level_label_ < integrity_level_ &&
      base::win::OSInfo::GetInstance()->version() >= base::win::VERSION_VISTA) {
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

  if (appcontainer_list_.get() && appcontainer_list_->HasAppContainer()) {
    // Windows refuses to work with an impersonation token. See SetAppContainer
    // implementation for more details.
    if (lockdown_level_ < USER_LIMITED || lockdown_level_ != initial_level_)
      return SBOX_ERROR_CANNOT_INIT_APPCONTAINER;

    *initial = INVALID_HANDLE_VALUE;
    return SBOX_ALL_OK;
  }

  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  result = CreateRestrictedToken(initial, initial_level_,
                                 integrity_level_, IMPERSONATION);
  if (ERROR_SUCCESS != result) {
    ::CloseHandle(*lockdown);
    return SBOX_ERROR_GENERIC;
  }
  return SBOX_ALL_OK;
}

const AppContainerAttributes* PolicyBase::GetAppContainer() {
  if (!appcontainer_list_.get() || !appcontainer_list_->HasAppContainer())
    return NULL;

  return appcontainer_list_.get();
}

bool PolicyBase::AddTarget(TargetProcess* target) {
  if (NULL != policy_)
    policy_maker_->Done();

  if (!ApplyProcessMitigationsToSuspendedProcess(target->Process(),
                                                 mitigations_)) {
    return false;
  }

  if (!SetupAllInterceptions(target))
    return false;

  if (!SetupHandleCloser(target))
    return false;

  // Initialize the sandbox infrastructure for the target.
  if (ERROR_SUCCESS != target->Init(this, policy_, kIPCMemSize, kPolMemSize))
    return false;

  g_shared_delayed_integrity_level = delayed_integrity_level_;
  ResultCode ret = target->TransferVariable(
                       "g_shared_delayed_integrity_level",
                       &g_shared_delayed_integrity_level,
                       sizeof(g_shared_delayed_integrity_level));
  g_shared_delayed_integrity_level = INTEGRITY_LEVEL_LAST;
  if (SBOX_ALL_OK != ret)
    return false;

  // Add in delayed mitigations and pseudo-mitigations enforced at startup.
  g_shared_delayed_mitigations = delayed_mitigations_ |
      FilterPostStartupProcessMitigations(mitigations_);
  if (!CanSetProcessMitigationsPostStartup(g_shared_delayed_mitigations))
    return false;

  ret = target->TransferVariable("g_shared_delayed_mitigations",
                                 &g_shared_delayed_mitigations,
                                 sizeof(g_shared_delayed_mitigations));
  g_shared_delayed_mitigations = 0;
  if (SBOX_ALL_OK != ret)
    return false;

  AutoLock lock(&lock_);
  targets_.push_back(target);
  return true;
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

// We service IPC_PING_TAG message which is a way to test a round trip of the
// IPC subsystem. We receive a integer cookie and we are expected to return the
// cookie times two (or three) and the current tick count.
bool PolicyBase::Ping(IPCInfo* ipc, void* arg1) {
  switch (ipc->ipc_tag) {
    case IPC_PING1_TAG: {
      IPCInt ipc_int(arg1);
      uint32 cookie = ipc_int.As32Bit();
      ipc->return_info.extended_count = 2;
      ipc->return_info.extended[0].unsigned_int = ::GetTickCount();
      ipc->return_info.extended[1].unsigned_int = 2 * cookie;
      return true;
    }
    case IPC_PING2_TAG: {
      CountedBuffer* io_buffer = reinterpret_cast<CountedBuffer*>(arg1);
      if (sizeof(uint32) != io_buffer->Size())
        return false;

      uint32* cookie = reinterpret_cast<uint32*>(io_buffer->Buffer());
      *cookie = (*cookie) * 3;
      return true;
    }
    default: return false;
  }
}

Dispatcher* PolicyBase::GetDispatcher(int ipc_tag) {
  if (ipc_tag >= IPC_LAST_TAG || ipc_tag <= IPC_UNUSED_TAG)
    return NULL;

  return ipc_targets_[ipc_tag];
}

bool PolicyBase::SetupAllInterceptions(TargetProcess* target) {
  InterceptionManager manager(target, relaxed_interceptions_);

  if (policy_) {
    for (int i = 0; i < IPC_LAST_TAG; i++) {
      if (policy_->entry[i] && !ipc_targets_[i]->SetupService(&manager, i))
          return false;
    }
  }

  if (!blacklisted_dlls_.empty()) {
    std::vector<base::string16>::iterator it = blacklisted_dlls_.begin();
    for (; it != blacklisted_dlls_.end(); ++it) {
      manager.AddToUnloadModules(it->c_str());
    }
  }

  if (!SetupBasicInterceptions(&manager))
    return false;

  if (!manager.InitializeInterceptions())
    return false;

  // Finally, setup imports on the target so the interceptions can work.
  return SetupNtdllImports(target);
}

bool PolicyBase::SetupHandleCloser(TargetProcess* target) {
  return handle_closer_.InitializeTargetHandles(target);
}

}  // namespace sandbox
