// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/broker_services.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox.h"
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
sandbox::ResultCode SpawnCleanup(sandbox::TargetProcess* target, DWORD error) {
  if (0 == error)
    error = ::GetLastError();

  target->Terminate();
  delete target;
  ::SetLastError(error);
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
  HANDLE job;
  sandbox::PolicyBase* policy;
  JobTracker(HANDLE cjob, sandbox::PolicyBase* cpolicy)
      : job(cjob), policy(cpolicy) {
  }
};

// Helper structure that allows the broker to track peer processes
struct PeerTracker {
  HANDLE wait_object;
  base::win::ScopedHandle process;
  DWORD id;
  HANDLE job_port;
  PeerTracker(DWORD process_id, HANDLE broker_job_port)
      : wait_object(NULL), id(process_id), job_port(broker_job_port) {
  }
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

BrokerServicesBase::BrokerServicesBase()
    : thread_pool_(NULL), job_port_(NULL), no_targets_(NULL),
      job_thread_(NULL) {
}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::Init() {
  if ((NULL != job_port_) || (NULL != thread_pool_))
    return SBOX_ERROR_UNEXPECTED_CALL;

  ::InitializeCriticalSection(&lock_);

  job_port_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (NULL == job_port_)
    return SBOX_ERROR_GENERIC;

  no_targets_ = ::CreateEventW(NULL, TRUE, FALSE, NULL);

  job_thread_ = ::CreateThread(NULL, 0,  // Default security and stack.
                               TargetEventsThread, this, NULL, NULL);
  if (NULL == job_thread_)
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
  if (!job_port_)
    return;

  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_, 0, THREAD_CTRL_QUIT, FALSE);
  ::CloseHandle(job_port_);

  if (WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_, 1000)) {
    // Cannot clean broker services.
    NOTREACHED();
    return;
  }

  JobTrackerList::iterator it;
  for (it = tracker_list_.begin(); it != tracker_list_.end(); ++it) {
    JobTracker* tracker = (*it);
    FreeResources(tracker);
    delete tracker;
  }
  ::CloseHandle(job_thread_);
  delete thread_pool_;
  ::CloseHandle(no_targets_);

  // Cancel the wait events and delete remaining peer trackers.
  for (PeerTrackerMap::iterator it = peer_map_.begin();
       it != peer_map_.end(); ++it) {
    DeregisterPeerTracker(it->second);
  }

  // If job_port_ isn't NULL, assumes that the lock has been initialized.
  if (job_port_)
    ::DeleteCriticalSection(&lock_);
}

TargetPolicy* BrokerServicesBase::CreatePolicy() {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  return new PolicyBase;
}

void BrokerServicesBase::FreeResources(JobTracker* tracker) {
  if (NULL != tracker->policy) {
    BOOL res = ::TerminateJobObject(tracker->job, SBOX_ALL_OK);
    DCHECK(res);
    // Closing the job causes the target process to be destroyed so this
    // needs to happen before calling OnJobEmpty().
    res = ::CloseHandle(tracker->job);
    DCHECK(res);
    // In OnJobEmpty() we don't actually use the job handle directly.
    tracker->policy->OnJobEmpty(tracker->job);
    tracker->policy->Release();
    tracker->policy = NULL;
  }
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
  HANDLE port = broker->job_port_;
  HANDLE no_targets = broker->no_targets_;

  int target_counter = 0;
  ::ResetEvent(no_targets);

  while (true) {
    DWORD events = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = NULL;

    if (!::GetQueuedCompletionStatus(port, &events, &key, &ovl, INFINITE))
      // this call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;

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
          FreeResources(tracker);
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          ++target_counter;
          if (1 == target_counter) {
            ::ResetEvent(no_targets);
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          {
            AutoLock lock(&broker->lock_);
            broker->child_process_ids_.erase(reinterpret_cast<DWORD>(ovl));
          }
          --target_counter;
          if (0 == target_counter)
            ::SetEvent(no_targets);

          DCHECK(target_counter >= 0);
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
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
      PeerTrackerMap::iterator it =
          broker->peer_map_.find(reinterpret_cast<DWORD>(ovl));
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
                                           TargetPolicy* policy,
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

  AutoLock lock(&lock_);

  // This downcast is safe as long as we control CreatePolicy()
  PolicyBase* policy_base = static_cast<PolicyBase*>(policy);

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  HANDLE initial_token_temp;
  HANDLE lockdown_token_temp;
  ResultCode result = policy_base->MakeTokens(&initial_token_temp,
                                              &lockdown_token_temp);
  if (SBOX_ALL_OK != result)
    return result;

  base::win::ScopedHandle initial_token(initial_token_temp);
  base::win::ScopedHandle lockdown_token(lockdown_token_temp);

  HANDLE job_temp;
  result = policy_base->MakeJobObject(&job_temp);
  if (SBOX_ALL_OK != result)
    return result;

  base::win::ScopedHandle job(job_temp);

  // Initialize the startup information from the policy.
  base::win::StartupInformation startup_info;
  string16 desktop = policy_base->GetAlternateDesktop();
  if (!desktop.empty()) {
    startup_info.startup_info()->lpDesktop =
        const_cast<wchar_t*>(desktop.c_str());
  }

  bool inherit_handles = false;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    int attribute_count = 0;
    const AppContainerAttributes* app_container =
        policy_base->GetAppContainer();
    if (app_container)
      ++attribute_count;

    DWORD64 mitigations;
    size_t mitigations_size;
    ConvertProcessMitigationsToPolicy(policy->GetProcessMitigations(),
                                      &mitigations, &mitigations_size);
    if (mitigations)
      ++attribute_count;

    HANDLE stdout_handle = policy_base->GetStdoutHandle();
    HANDLE stderr_handle = policy_base->GetStderrHandle();
    HANDLE inherit_handle_list[2];
    int inherit_handle_count = 0;
    if (stdout_handle != INVALID_HANDLE_VALUE)
      inherit_handle_list[inherit_handle_count++] = stdout_handle;
    // Handles in the list must be unique.
    if (stderr_handle != stdout_handle && stderr_handle != INVALID_HANDLE_VALUE)
      inherit_handle_list[inherit_handle_count++] = stderr_handle;
    if (inherit_handle_count)
      ++attribute_count;

    if (!startup_info.InitializeProcThreadAttributeList(attribute_count))
      return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;

    if (app_container) {
      result = app_container->ShareForStartup(&startup_info);
      if (SBOX_ALL_OK != result)
        return result;
    }

    if (mitigations) {
      if (!startup_info.UpdateProcThreadAttribute(
               PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &mitigations,
               mitigations_size)) {
        return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;
      }
    }

    if (inherit_handle_count) {
      if (!startup_info.UpdateProcThreadAttribute(
              PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
              inherit_handle_list,
              sizeof(inherit_handle_list[0]) * inherit_handle_count)) {
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
  } else if (getenv("MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA")) {
    // On pre-Vista versions even if we can't limit what gets inherited, we
    // sometimes want to inherit stdout/err for testing purposes.
    startup_info.startup_info()->dwFlags |= STARTF_USESTDHANDLES;
    startup_info.startup_info()->hStdInput = INVALID_HANDLE_VALUE;
    startup_info.startup_info()->hStdOutput = policy_base->GetStdoutHandle();
    startup_info.startup_info()->hStdError = policy_base->GetStderrHandle();
    inherit_handles = true;
  }

  // Construct the thread pool here in case it is expensive.
  // The thread pool is shared by all the targets
  if (NULL == thread_pool_)
    thread_pool_ = new Win2kThreadPool();

  // Create the TargetProces object and spawn the target suspended. Note that
  // Brokerservices does not own the target object. It is owned by the Policy.
  base::win::ScopedProcessInformation process_info;
  TargetProcess* target = new TargetProcess(initial_token.Take(),
                                            lockdown_token.Take(),
                                            job,
                                            thread_pool_);

  DWORD win_result = target->Create(exe_path, command_line, inherit_handles,
                                    startup_info, &process_info);
  if (ERROR_SUCCESS != win_result)
    return SpawnCleanup(target, win_result);

  // Now the policy is the owner of the target.
  if (!policy_base->AddTarget(target)) {
    return SpawnCleanup(target, 0);
  }

  // We are going to keep a pointer to the policy because we'll call it when
  // the job object generates notifications using the completion port.
  policy_base->AddRef();
  if (job.IsValid()) {
    scoped_ptr<JobTracker> tracker(new JobTracker(job.Take(), policy_base));
    if (!AssociateCompletionPort(tracker->job, job_port_, tracker.get()))
      return SpawnCleanup(target, 0);
    // Save the tracker because in cleanup we might need to force closing
    // the Jobs.
    tracker_list_.push_back(tracker.release());
    child_process_ids_.insert(process_info.process_id());
  } else {
    // We have to signal the event once here because the completion port will
    // never get a message that this target is being terminated thus we should
    // not block WaitForAllTargets until we have at least one target with job.
    if (child_process_ids_.empty())
      ::SetEvent(no_targets_);
    // We can not track the life time of such processes and it is responsibility
    // of the host application to make sure that spawned targets without jobs
    // are terminated when the main application don't need them anymore.
  }

  *target_info = process_info.Take();
  return SBOX_ALL_OK;
}


ResultCode BrokerServicesBase::WaitForAllTargets() {
  ::WaitForSingleObject(no_targets_, INFINITE);
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
  ::PostQueuedCompletionStatus(peer->job_port, 0, THREAD_CTRL_REMOVE_PEER,
                               reinterpret_cast<LPOVERLAPPED>(peer->id));
}

ResultCode BrokerServicesBase::AddTargetPeer(HANDLE peer_process) {
  scoped_ptr<PeerTracker> peer(new PeerTracker(::GetProcessId(peer_process),
                                               job_port_));
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
          &peer->wait_object, peer->process, RemovePeer, peer.get(), INFINITE,
          WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD)) {
    peer_map_.erase(peer->id);
    return SBOX_ERROR_GENERIC;
  }

  // Release the pointer since it will be cleaned up by the callback.
  peer.release();
  return SBOX_ALL_OK;
}

ResultCode BrokerServicesBase::InstallAppContainer(const wchar_t* sid,
                                                   const wchar_t* name) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return SBOX_ERROR_UNSUPPORTED;

  string16 old_name = LookupAppContainer(sid);
  if (old_name.empty())
    return CreateAppContainer(sid, name);

  if (old_name != name)
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  return SBOX_ALL_OK;
}

ResultCode BrokerServicesBase::UninstallAppContainer(const wchar_t* sid) {
  if (base::win::OSInfo::GetInstance()->version() < base::win::VERSION_WIN8)
    return SBOX_ERROR_UNSUPPORTED;

  string16 name =  LookupAppContainer(sid);
  if (name.empty())
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  return DeleteAppContainer(sid);
}

}  // namespace sandbox
