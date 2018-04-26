/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Scheduler_h
#define mozilla_Scheduler_h

#include "mozilla/Attributes.h"
#include "mozilla/EventQueue.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"
#include "nsILabelableRunnable.h"

// Windows silliness. winbase.h defines an empty no-argument Yield macro.
#undef Yield

class nsIBlockThreadedExecutionCallback;
class nsIIdlePeriod;
class nsThread;

namespace mozilla {

class SchedulerGroup;
class SchedulerImpl;
class SynchronizedEventQueue;

// This is the central class for scheduling work on the "main" thread. It starts
// a pool of cooperatively scheduled threads (using CooperativeThreadPool) and
// controls them using a single, main-thread event queue
// (SchedulerEventQueue). Even if cooperative scheduling is not enabled,
// Scheduler can schedule work on the main thread. Its behavior is controlled by
// a number of preferences:
//
// XXX The cooperative scheduler is disabled because it will crash immediately.
//
// "dom.ipc.scheduler.useMultipleQueues": When this pref is true, a
// LabeledEventQueue is used for the main thread event queue. This divides the
// event queue into multiple queues, one per SchedulerGroup. If the pref is
// false, a normal EventQueue is used. Either way, event prioritization via
// PrioritizedEventQueue still happens.
//
// "dom.ipc.scheduler.preemption": If this pref is true, then cooperative
// threads can be preempted before they have finished. This might happen if a
// different cooperative thread is running an event for a higher priority
// SchedulerGroup.
//
// "dom.ipc.scheduler.threadCount": The number of cooperative threads to start.
//
// "dom.ipc.scheduler.chaoticScheduling": When this pref is set, we make an
// effort to switch between threads even when it is not necessary to do
// this. This is useful for debugging.
class Scheduler
{
public:
  static already_AddRefed<nsThread> Init(nsIIdlePeriod* aIdlePeriod);
  static void Start();
  static void Shutdown();

  // Scheduler prefs need to be handled differently because the scheduler needs
  // to start up in the content process before the normal preferences service.
  static nsCString GetPrefs();
  static void SetPrefs(const char* aPrefs);

  static bool IsSchedulerEnabled();
  static bool UseMultipleQueues();

  static bool IsCooperativeThread();

  static void Yield();

  static bool UnlabeledEventRunning();
  static bool AnyEventRunning();

  static void BlockThreadedExecution(nsIBlockThreadedExecutionCallback* aCallback);
  static void UnblockThreadedExecution();

  class MOZ_RAII EventLoopActivation
  {
  public:
    using EventGroups = nsILabelableRunnable::SchedulerGroupSet;
    EventLoopActivation();
    ~EventLoopActivation();

    static void Init();

    bool IsNested() const { return !!mPrev; }

    void SetEvent(nsIRunnable* aEvent, EventPriority aPriority);

    EventPriority Priority() const { return mPriority; }
    bool IsLabeled() { return mIsLabeled; }
    EventGroups& EventGroupsAffected() { return mEventGroups; }

  private:
    EventLoopActivation* mPrev;
    bool mProcessingEvent;
    bool mIsLabeled;
    EventGroups mEventGroups;
    EventPriority mPriority;

    static MOZ_THREAD_LOCAL(EventLoopActivation*) sTopActivation;
  };

private:
  friend class SchedulerImpl;
  static UniquePtr<SchedulerImpl> sScheduler;
};

} // namespace mozilla

#endif // mozilla_Scheduler_h
