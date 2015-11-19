/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/source/process_thread_impl.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
namespace {

// We use this constant internally to signal that a module has requested
// a callback right away.  When this is set, no call to TimeUntilNextProcess
// should be made, but Process() should be called directly.
const int64_t kCallProcessImmediately = -1;

int64_t GetNextCallbackTime(Module* module, int64_t time_now) {
  int64_t interval = module->TimeUntilNextProcess();
  // Currently some implementations erroneously return error codes from
  // TimeUntilNextProcess(). So, as is, we correct that and log an error.
  if (interval < 0) {
    LOG(LS_ERROR) << "TimeUntilNextProcess returned an invalid value "
                  << interval;
    interval = 0;
  }
  return time_now + interval;
}
}

ProcessThread::~ProcessThread() {}

// static
rtc::scoped_ptr<ProcessThread> ProcessThread::Create() {
  return rtc::scoped_ptr<ProcessThread>(new ProcessThreadImpl()).Pass();
}

ProcessThreadImpl::ProcessThreadImpl()
    : wake_up_(EventWrapper::Create()), stop_(false) {
}

ProcessThreadImpl::~ProcessThreadImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!thread_.get());
  DCHECK(!stop_);

  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop();
  }
}

void ProcessThreadImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!thread_.get());
  if (thread_.get())
    return;

  DCHECK(!stop_);

  {
    // TODO(tommi): Since DeRegisterModule is currently being called from
    // different threads in some cases (ChannelOwner), we need to lock access to
    // the modules_ collection even on the controller thread.
    // Once we've cleaned up those places, we can remove this lock.
    rtc::CritScope lock(&lock_);
    for (ModuleCallback& m : modules_)
      m.module->ProcessThreadAttached(this);
  }

  thread_ = ThreadWrapper::CreateThread(
      &ProcessThreadImpl::Run, this, "ProcessThread");
  CHECK(thread_->Start());
}

void ProcessThreadImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if(!thread_.get())
    return;

  {
    rtc::CritScope lock(&lock_);
    stop_ = true;
  }

  wake_up_->Set();

  CHECK(thread_->Stop());
  stop_ = false;

  // TODO(tommi): Since DeRegisterModule is currently being called from
  // different threads in some cases (ChannelOwner), we need to lock access to
  // the modules_ collection even on the controller thread.
  // Since DeRegisterModule also checks thread_, we also need to hold the
  // lock for the .reset() operation.
  // Once we've cleaned up those places, we can remove this lock.
  rtc::CritScope lock(&lock_);
  thread_.reset();
  for (ModuleCallback& m : modules_)
    m.module->ProcessThreadAttached(nullptr);
}

void ProcessThreadImpl::WakeUp(Module* module) {
  // Allowed to be called on any thread.
  {
    rtc::CritScope lock(&lock_);
    for (ModuleCallback& m : modules_) {
      if (m.module == module)
        m.next_callback = kCallProcessImmediately;
    }
  }
  wake_up_->Set();
}

void ProcessThreadImpl::PostTask(rtc::scoped_ptr<ProcessTask> task) {
  // Allowed to be called on any thread.
  {
    rtc::CritScope lock(&lock_);
    queue_.push(task.release());
  }
  wake_up_->Set();
}

void ProcessThreadImpl::RegisterModule(Module* module) {
  //  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(module);

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
  {
    // Catch programmer error.
    rtc::CritScope lock(&lock_);
    for (const ModuleCallback& mc : modules_)
      DCHECK(mc.module != module);
  }
#endif

  // Now that we know the module isn't in the list, we'll call out to notify
  // the module that it's attached to the worker thread.  We don't hold
  // the lock while we make this call.
  if (thread_.get())
    module->ProcessThreadAttached(this);

  {
    rtc::CritScope lock(&lock_);
    modules_.push_back(ModuleCallback(module));
  }

  // Wake the thread calling ProcessThreadImpl::Process() to update the
  // waiting time. The waiting time for the just registered module may be
  // shorter than all other registered modules.
  wake_up_->Set();
}

void ProcessThreadImpl::DeRegisterModule(Module* module) {
  // Allowed to be called on any thread.
  // TODO(tommi): Disallow this ^^^
  DCHECK(module);

  {
    rtc::CritScope lock(&lock_);
    modules_.remove_if([&module](const ModuleCallback& m) {
        return m.module == module;
      });

    // TODO(tommi): we currently need to hold the lock while calling out to
    // ProcessThreadAttached.  This is to make sure that the thread hasn't been
    // destroyed while we attach the module.  Once we can make sure
    // DeRegisterModule isn't being called on arbitrary threads, we can move the
    // |if (thread_.get())| check and ProcessThreadAttached() call outside the
    // lock scope.

    // Notify the module that it's been detached.
    if (thread_.get())
      module->ProcessThreadAttached(nullptr);
  }
}

// static
bool ProcessThreadImpl::Run(void* obj) {
  return static_cast<ProcessThreadImpl*>(obj)->Process();
}

bool ProcessThreadImpl::Process() {
  int64_t now = TickTime::MillisecondTimestamp();
  int64_t next_checkpoint = now + (1000 * 60);

  {
    rtc::CritScope lock(&lock_);
    if (stop_)
      return false;
    for (ModuleCallback& m : modules_) {
      // TODO(tommi): Would be good to measure the time TimeUntilNextProcess
      // takes and dcheck if it takes too long (e.g. >=10ms).  Ideally this
      // operation should not require taking a lock, so querying all modules
      // should run in a matter of nanoseconds.
      if (m.next_callback == 0)
        m.next_callback = GetNextCallbackTime(m.module, now);

      if (m.next_callback <= now ||
          m.next_callback == kCallProcessImmediately) {
        m.module->Process();
        // Use a new 'now' reference to calculate when the next callback
        // should occur.  We'll continue to use 'now' above for the baseline
        // of calculating how long we should wait, to reduce variance.
        int64_t new_now = TickTime::MillisecondTimestamp();
        m.next_callback = GetNextCallbackTime(m.module, new_now);
      }

      if (m.next_callback < next_checkpoint)
        next_checkpoint = m.next_callback;
    }

    while (!queue_.empty()) {
      ProcessTask* task = queue_.front();
      queue_.pop();
      lock_.Leave();
      task->Run();
      delete task;
      lock_.Enter();
    }
  }

  int64_t time_to_wait = next_checkpoint - TickTime::MillisecondTimestamp();
  if (time_to_wait > 0)
    wake_up_->Wait(static_cast<unsigned long>(time_to_wait));

  return true;
}
}  // namespace webrtc
