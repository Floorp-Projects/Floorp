// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/broker_services.h"

#include <AclAPI.h>
#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/win2k_threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// Utility function to associate a completion port to a job object.
bool AssociateCompletionPort(HANDLE job, HANDLE port, void* key) {
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_acp = { key, port };
  return ::SetInformationJobObject(job,
                                   JobObjectAssociateCompletionPortInformation,
                                   &job_acp, sizeof(job_acp))? true : false;
}

// Utility function to do the cleanup necessary when something goes wrong
// while in SpawnTarget and we must terminate the target process.
sandbox::ResultCode SpawnCleanup(sandbox::TargetProcess* target) {
  target->Terminate();
  delete target;
  return sandbox::SBOX_ERROR_GENERIC;
}

// the different commands that you can send to the worker thread that
// executes TargetEventsThread().
enum {
  THREAD_CTRL_NONE,
  THREAD_CTRL_REMOVE_PEER,
  THREAD_CTRL_QUIT,
  THREAD_CTRL_LAST,
};

// Helper structure that allows the Broker to associate a job notification
// with a job object and with a policy.
struct JobTracker {
  JobTracker(base::win::ScopedHandle job,
             scoped_refptr<sandbox::PolicyBase> policy)
      : job(std::move(job)), policy(policy) {}
  ~JobTracker() {
    FreeResources();
  }

  // Releases the Job and notifies the associated Policy object to release its
  // resources as well.
  void FreeResources();

  base::win::ScopedHandle job;
  scoped_refptr<sandbox::PolicyBase> policy;
};

void JobTracker::FreeResources() {
  if (policy) {
    BOOL res = ::TerminateJobObject(job.Get(), sandbox::SBOX_ALL_OK);
    DCHECK(res);
    // Closing the job causes the target process to be destroyed so this needs
    // to happen before calling OnJobEmpty().
    HANDLE stale_job_handle = job.Get();
    job.Close();

    // In OnJobEmpty() we don't actually use the job handle directly.
    policy->OnJobEmpty(stale_job_handle);
    policy = nullptr;
  }
}
 
// Helper structure that allows the broker to track peer processes
struct PeerTracker {
  PeerTracker(DWORD process_id, HANDLE broker_job_port)
      : wait_object(NULL), id(process_id), job_port(broker_job_port) {
  }

  HANDLE wait_object;
  base::win::ScopedHandle process;
  DWORD id;
  HANDLE job_port;
};

void DeregisterPeerTracker(PeerTracker* peer) {
  // Deregistration shouldn't fail, but we leak rather than crash if it does.
  if (::UnregisterWaitEx(peer->wait_object, INVALID_HANDLE_VALUE)) {
    delete peer;
  } else {
    NOTREACHED();
  }
}

}  // namespace

namespace sandbox {

BrokerServicesBase::BrokerServicesBase() {}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::Init() {
  if (job_port_.IsValid() || thread_pool_)
    return SBOX_ERROR_UNEXPECTED_CALL;

  ::InitializeCriticalSection(&lock_);

  job_port_.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0));
  if (!job_port_.IsValid())
    return SBOX_ERROR_GENERIC;

  no_targets_.Set(::CreateEventW(NULL, TRUE, FALSE, NULL));

  job_thread_.Set(::CreateThread(NULL, 0,  // Default security and stack.
                                 TargetEventsThread, this, NULL, NULL));
  if (!job_thread_.IsValid())
    return SBOX_ERROR_GENERIC;

  return SBOX_ALL_OK;
}

// The destructor should only be called when the Broker process is terminating.
// Since BrokerServicesBase is a singleton, this is called from the CRT
// termination handlers, if this code lives on a DLL it is called during
// DLL_PROCESS_DETACH in other words, holding the loader lock, so we cannot
// wait for threads here.
BrokerServicesBase::~BrokerServicesBase() {
  // If there is no port Init() was never called successfully.
  if (!job_port_.IsValid())
    return;

  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_.Get(), 0, THREAD_CTRL_QUIT, FALSE);

  if (job_thread_.IsValid() &&
      WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_.Get(), 1000)) {
    // Cannot clean broker services.
    NOTREACHED();
    return;
  }

  tracker_list_.clear();
  thread_pool_.reset();

  // Cancel the wait events and delete remaining peer trackers.
  for (PeerTrackerMap::iterator it = peer_map_.begin();
       it != peer_map_.end(); ++it) {
    DeregisterPeerTracker(it->second);
  }

  ::DeleteCriticalSection(&lock_);
}

scoped_refptr<TargetPolicy> BrokerServicesBase::CreatePolicy() {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  scoped_refptr<TargetPolicy> policy(new PolicyBase);
  // PolicyBase starts with refcount 1.
  policy->Release();
  return policy;
}

// The worker thread stays in a loop waiting for asynchronous notifications
// from the job objects. Right now we only care about knowing when the last
// process on a job terminates, but in general this is the place to tell
// the policy about events.
DWORD WINAPI BrokerServicesBase::TargetEventsThread(PVOID param) {
  if (NULL == param)
    return 1;

  base::PlatformThread::SetName("BrokerEvent");

  BrokerServicesBase* broker = reinterpret_cast<BrokerServicesBase*>(param);
  HANDLE port = broker->job_port_.Get();
  HANDLE no_targets = broker->no_targets_.Get();

  int target_counter = 0;
  int untracked_target_counter = 0;
  ::ResetEvent(no_targets);

  while (true) {
    DWORD events = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = NULL;

    if (!::GetQueuedCompletionStatus(port, &events, &key, &ovl, INFINITE)) {
      // this call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;
    }

    if (key > THREAD_CTRL_LAST) {
      // The notification comes from a job object. There are nine notifications
      // that jobs can send and some of them depend on the job attributes set.
      JobTracker* tracker = reinterpret_cast<JobTracker*>(key);

      switch (events) {
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
          // The job object has signaled that the last process associated
          // with it has terminated. Assuming there is no way for a process
          // to appear out of thin air in this job, it safe to assume that
          // we can tell the policy to destroy the target object, and for
          // us to release our reference to the policy object.
          tracker->FreeResources();
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          DWORD handle = static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl));
          {
            AutoLock lock(&broker->lock_);
            size_t count = broker->child_process_ids_.count(handle);
            // Child process created from sandboxed process.
            if (count == 0)
              untracked_target_counter++;
          }
          ++target_counter;
          if (1 == target_counter) {
            ::ResetEvent(no_targets);
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          size_t erase_result = 0;
          {
            AutoLock lock(&broker->lock_);
            erase_result = broker->child_process_ids_.erase(
                static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl)));
          }
          if (erase_result != 1U) {
            // The process was untracked e.g. a child process of the target.
            --untracked_target_counter;
            DCHECK(untracked_target_counter >= 0);
          }
          --target_counter;
          if (0 == target_counter)
            ::SetEvent(no_targets);

          DCHECK(target_counter >= 0);
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
          // A child process attempted and failed to create a child process.
          // Windows does not reveal the process id.
          untracked_target_counter++;
          target_counter++;
          break;
        }

        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: {
          BOOL res = ::TerminateJobObject(tracker->job.Get(),
                                          SBOX_FATAL_MEMORY_EXCEEDED);
          DCHECK(res);
          break;
        }

        default: {
          NOTREACHED();
          break;
        }
      }
    } else if (THREAD_CTRL_REMOVE_PEER == key) {
      // Remove a process from our list of peers.
      AutoLock lock(&broker->lock_);
      PeerTrackerMap::iterator it = broker->peer_map_.find(
          static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl)));
      DeregisterPeerTracker(it->second);
      broker->peer_map_.erase(it);
    } else if (THREAD_CTRL_QUIT == key) {
      // The broker object is being destroyed so the thread needs to exit.
      return 0;
    } else {
      // We have not implemented more commands.
      NOTREACHED();
    }
  }

  NOTREACHED();
  return 0;
}

// SpawnTarget does all the interesting sandbox setup and creates the target
// process inside the sandbox.
ResultCode BrokerServicesBase::SpawnTarget(const wchar_t* exe_path,
                                           const wchar_t* command_line,
                                           scoped_refptr<TargetPolicy> policy,
                                           ResultCode* last_warning,
                                           DWORD* last_error,
                                           PROCESS_INFORMATION* target_info) {
  if (!exe_path)
    return SBOX_ERROR_BAD_PARAMS;

  if (!policy)
    return SBOX_ERROR_BAD_PARAMS;

  // Even though the resources touched by SpawnTarget can be accessed in
  // multiple threads, the method itself cannot be called from more than
  // 1 thread. This is to protect the global variables used while setting up
  // the child process.
  static DWORD thread_id = ::GetCurrentThreadId();
  DCHECK(thread_id == ::GetCurrentThreadId());
  *last_warning = SBOX_ALL_OK;

  AutoLock lock(&lock_);

  // This downcast is safe as long as we control CreatePolicy()
  scoped_refptr<PolicyBase> policy_base(static_cast<PolicyBase*>(policy.get()));

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  base::win::ScopedHandle initial_token;
  base::win::ScopedHandle lockdown_token;
  base::win::ScopedHandle lowbox_token;
  ResultCode result = SBOX_ALL_OK;

  result =
      policy_base->MakeTokens(&initial_token, &lockdown_token, &lowbox_token);
  if (SBOX_ALL_OK != result)
    return result;
  if (lowbox_token.IsValid() &&
      base::win::GetVersion() < base::win::VERSION_WIN8) {
    // We don't allow lowbox_token below Windows 8.
    return SBOX_ERROR_BAD_PARAMS;
  }

  base::win::ScopedHandle job;
  result = policy_base->MakeJobObject(&job);
  if (SBOX_ALL_OK != result)
    return result;

  // Initialize the startup information from the policy.
  base::win::StartupInformation startup_info;

  // We don't want any child processes causing the IDC_APPSTARTING cursor.
  startup_info.startup_info()->dwFlags |= STARTF_FORCEOFFFEEDBACK;

  // The liftime of |mitigations|, |inherit_handle_list| and
  // |child_process_creation| have to be at least as long as
  // |startup_info| because |UpdateProcThreadAttribute| requires that
  // its |lpValue| parameter persist until |DeleteProcThreadAttributeList| is
  // called; StartupInformation's destructor makes such a call.
  DWORD64 mitigations;
  std::vector<HANDLE> inherited_handle_list;
  DWORD child_process_creation = PROCESS_CREATION_CHILD_PROCESS_RESTRICTED;

  base::string16 desktop = policy_base->GetAlternateDesktop();
  if (!desktop.empty()) {
    startup_info.startup_info()->lpDesktop =
        const_cast<wchar_t*>(desktop.c_str());
  }

  bool inherit_handles = false;

  int attribute_count = 0;

  size_t mitigations_size;
  ConvertProcessMitigationsToPolicy(policy_base->GetProcessMitigations(),
                                    &mitigations, &mitigations_size);
  if (mitigations)
    ++attribute_count;

  bool restrict_child_process_creation = false;
  if (base::win::GetVersion() >= base::win::VERSION_WIN10_TH2 &&
      policy_base->GetJobLevel() <= JOB_LIMITED_USER) {
    restrict_child_process_creation = true;
    ++attribute_count;
  }

  HANDLE stdout_handle = policy_base->GetStdoutHandle();
  HANDLE stderr_handle = policy_base->GetStderrHandle();

  if (stdout_handle != INVALID_HANDLE_VALUE)
    inherited_handle_list.push_back(stdout_handle);

  // Handles in the list must be unique.
  if (stderr_handle != stdout_handle && stderr_handle != INVALID_HANDLE_VALUE)
    inherited_handle_list.push_back(stderr_handle);

  const auto& policy_handle_list = policy_base->GetHandlesBeingShared();

  for (HANDLE handle : policy_handle_list)
    inherited_handle_list.push_back(handle);

  if (inherited_handle_list.size())
    ++attribute_count;

  if (!startup_info.InitializeProcThreadAttributeList(attribute_count))
    return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;

  if (mitigations) {
    if (!startup_info.UpdateProcThreadAttribute(
              PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &mitigations,
              mitigations_size)) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
  }

  if (restrict_child_process_creation) {
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY,
            &child_process_creation, sizeof(child_process_creation))) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
  }

  if (inherited_handle_list.size()) {
    if (!startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            &inherited_handle_list[0],
            sizeof(HANDLE) * inherited_handle_list.size())) {
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
    }
    startup_info.startup_info()->dwFlags |= STARTF_USESTDHANDLES;
    startup_info.startup_info()->hStdInput = INVALID_HANDLE_VALUE;
    startup_info.startup_info()->hStdOutput = stdout_handle;
    startup_info.startup_info()->hStdError = stderr_handle;
    // Allowing inheritance of handles is only secure now that we
    // have limited which handles will be inherited.
    inherit_handles = true;
  }

  // Construct the thread pool here in case it is expensive.
  // The thread pool is shared by all the targets
  if (!thread_pool_)
    thread_pool_ = base::MakeUnique<Win2kThreadPool>();

  // Create the TargetProcess object and spawn the target suspended. Note that
  // Brokerservices does not own the target object. It is owned by the Policy.
  base::win::ScopedProcessInformation process_info;
  TargetProcess* target =
      new TargetProcess(std::move(initial_token), std::move(lockdown_token),
                        job.Get(), thread_pool_.get());

  result = target->Create(exe_path, command_line, inherit_handles, startup_info,
                          &process_info, last_error);

  if (result != SBOX_ALL_OK) {
    SpawnCleanup(target);
    return result;
  }

  if (lowbox_token.IsValid()) {
    *last_warning = target->AssignLowBoxToken(lowbox_token);
    // If this fails we continue, but report the error as a warning.
    // This is due to certain configurations causing the setting of the
    // token to fail post creation, and we'd rather continue if possible.
    if (*last_warning != SBOX_ALL_OK)
      *last_error = ::GetLastError();
  }

  // Now the policy is the owner of the target.
  result = policy_base->AddTarget(target);

  if (result != SBOX_ALL_OK) {
    *last_error = ::GetLastError();
    SpawnCleanup(target);
    return result;
  }

  // We are going to keep a pointer to the policy because we'll call it when
  // the job object generates notifications using the completion port.
  if (job.IsValid()) {
    std::unique_ptr<JobTracker> tracker =
        base::MakeUnique<JobTracker>(std::move(job), policy_base);

    // There is no obvious recovery after failure here. Previous version with
    // SpawnCleanup() caused deletion of TargetProcess twice. crbug.com/480639
    CHECK(AssociateCompletionPort(tracker->job.Get(), job_port_.Get(),
                                  tracker.get()));

    // Save the tracker because in cleanup we might need to force closing
    // the Jobs.
    tracker_list_.push_back(std::move(tracker));
    child_process_ids_.insert(process_info.process_id());
  } else {
    // Leak policy_base. This needs to outlive the child process, but there's
    // nothing that tracks that lifetime properly if there's no job object.
    // TODO(wfh): Find a way to make this have the correct lifetime.
    policy_base->AddRef();

    // We have to signal the event once here because the completion port will
    // never get a message that this target is being terminated thus we should
    // not block WaitForAllTargets until we have at least one target with job.
    if (child_process_ids_.empty())
      ::SetEvent(no_targets_.Get());
    // We can not track the life time of such processes and it is responsibility
    // of the host application to make sure that spawned targets without jobs
    // are terminated when the main application don't need them anymore.
    // Sandbox policy engine needs to know that these processes are valid
    // targets for e.g. BrokerDuplicateHandle so track them as peer processes.
    AddTargetPeer(process_info.process_handle());
  }

  *target_info = process_info.Take();
  return result;
}


ResultCode BrokerServicesBase::WaitForAllTargets() {
  ::WaitForSingleObject(no_targets_.Get(), INFINITE);
  return SBOX_ALL_OK;
}

bool BrokerServicesBase::IsActiveTarget(DWORD process_id) {
  AutoLock lock(&lock_);
  return child_process_ids_.find(process_id) != child_process_ids_.end() ||
         peer_map_.find(process_id) != peer_map_.end();
}

VOID CALLBACK BrokerServicesBase::RemovePeer(PVOID parameter, BOOLEAN timeout) {
  PeerTracker* peer = reinterpret_cast<PeerTracker*>(parameter);
  // Don't check the return code because we this may fail (safely) at shutdown.
  ::PostQueuedCompletionStatus(
      peer->job_port, 0, THREAD_CTRL_REMOVE_PEER,
      reinterpret_cast<LPOVERLAPPED>(static_cast<uintptr_t>(peer->id)));
}

ResultCode BrokerServicesBase::AddTargetPeer(HANDLE peer_process) {
  std::unique_ptr<PeerTracker> peer(
      new PeerTracker(::GetProcessId(peer_process), job_port_.Get()));
  if (!peer->id)
    return SBOX_ERROR_GENERIC;

  HANDLE process_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), peer_process,
                         ::GetCurrentProcess(), &process_handle,
                         SYNCHRONIZE, FALSE, 0)) {
    return SBOX_ERROR_GENERIC;
  }
  peer->process.Set(process_handle);

  AutoLock lock(&lock_);
  if (!peer_map_.insert(std::make_pair(peer->id, peer.get())).second)
    return SBOX_ERROR_BAD_PARAMS;

  if (!::RegisterWaitForSingleObject(
          &peer->wait_object, peer->process.Get(), RemovePeer, peer.get(),
          INFINITE, WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD)) {
    peer_map_.erase(peer->id);
    return SBOX_ERROR_GENERIC;
  }

  // Release the pointer since it will be cleaned up by the callback.
  ignore_result(peer.release());
  return SBOX_ALL_OK;
}

}  // namespace sandbox
