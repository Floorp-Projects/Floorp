/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BackgroundHangMonitor_h
#define mozilla_BackgroundHangMonitor_h

#include "mozilla/RefPtr.h"
#include "mozilla/Monitor.h"

#include <stdint.h>

namespace mozilla {

namespace Telemetry {
class ThreadHangStats;
};

// Disabled for Beta/Release builds because of bug 965392.
// Disabled for debug builds because of bug 979069.
#if !defined(RELEASE_BUILD) && !defined(DEBUG)
// Undefine to disable background hang monitor
#define MOZ_ENABLE_BACKGROUND_HANG_MONITOR
#endif

class BackgroundHangThread;

/**
 * The background hang monitor is responsible for detecting and reporting
 * hangs in background (non-main) threads. A thread registers itself using
 * the BackgroundHangMonitor object and periodically calls its methods to
 * inform the hang monitor of the thread's activity. Each thread is given
 * a thread name, a timeout, and a maximum timeout. If one of the thread's
 * tasks runs for longer than the timeout duration but shorter than the
 * maximum timeout, a (transient) hang is reported. On the other hand, if
 * a task runs for longer than the maximum timeout duration or never
 * finishes (e.g. in a deadlock), a permahang is reported.
 *
 * Tasks are defined arbitrarily, but are typically represented by events
 * in an event loop -- processing one event is equivalent to running one
 * task. To ensure responsiveness, tasks in a thread often have a target
 * running time. This is a good starting point for determining the timeout
 * and maximum timeout values. For example, the Compositor thread has a
 * responsiveness goal of 60Hz or 17ms, so a starting timeout could be
 * 100ms. Considering some platforms (e.g. Android) can terminate the app
 * when a critical thread hangs for longer than a few seconds, a good
 * starting maximum timeout is 4 or 5 seconds.
 *
 * A thread registers itself through the BackgroundHangMonitor constructor.
 * Multiple BackgroundHangMonitor objects can be used in one thread. The
 * constructor without arguments can be used when it is known that the thread
 * already has a BackgroundHangMonitor registered. When all instances of
 * BackgroundHangMonitor are destroyed, the thread is unregistered.
 *
 * The thread then uses two methods to inform BackgroundHangMonitor of the
 * thread's activity:
 *
 *  > BackgroundHangMonitor::NotifyActivity should be called *before*
 *    starting a task. The task run time is determined by the interval
 *    between this call and the next NotifyActivity call.
 *
 *  > BackgroundHangMonitor::NotifyWait should be called *before* the
 *    thread enters a wait state (e.g. to wait for a new event). This
 *    prevents a waiting thread from being detected as hanging. The wait
 *    state is automatically cleared at the next NotifyActivity call.
 *
 * The following example shows hang monitoring in a simple event loop:
 *
 *  void thread_main()
 *  {
 *    mozilla::BackgroundHangMonitor hangMonitor("example1", 100, 1000);
 *    while (!exiting) {
 *      hangMonitor.NotifyActivity();
 *      process_next_event();
 *      hangMonitor.NotifyWait();
 *      wait_for_next_event();
 *    }
 *  }
 *
 * The following example shows reentrancy in nested event loops:
 *
 *  void thread_main()
 *  {
 *    mozilla::BackgroundHangMonitor hangMonitor("example2", 100, 1000);
 *    while (!exiting) {
 *      hangMonitor.NotifyActivity();
 *      process_next_event();
 *      hangMonitor.NotifyWait();
 *      wait_for_next_event();
 *    }
 *  }
 *
 *  void process_next_event()
 *  {
 *    mozilla::BackgroundHangMonitor hangMonitor();
 *    if (is_sync_event) {
 *      while (!finished_event) {
 *        hangMonitor.NotifyActivity();
 *        process_next_event();
 *        hangMonitor.NotifyWait();
 *        wait_for_next_event();
 *      }
 *    } else {
 *      process_nonsync_event();
 *    }
 *  }
 *
 */
class BackgroundHangMonitor
{
private:
  RefPtr<BackgroundHangThread> mThread;

public:
  static const uint32_t kNoTimeout = 0;

  /**
   * ThreadHangStatsIterator is used to iterate through the ThreadHangStats
   * associated with each active monitored thread. Because of an internal
   * lock while this object is alive, a thread must use only one instance
   * of this class at a time and must iterate through the list as fast as
   * possible. The following example shows using the iterator:
   *
   * {
   *   // Scope the iter variable so it's destroyed as soon as we're done
   *   BackgroundHangMonitor::ThreadHangStatsIterator iter;
   *   for (ThreadHangStats* histogram = iter.GetNext();
   *        histogram; histogram = iter.GetNext()) {
   *     // Process histogram
   *   }
   * }
   */
  class ThreadHangStatsIterator : public MonitorAutoLock
  {
  private:
    BackgroundHangThread* mThread;

    ThreadHangStatsIterator(const ThreadHangStatsIterator&);
    ThreadHangStatsIterator& operator=(const ThreadHangStatsIterator&);

  public:
    /**
     * Create an ThreadHangStatsIterator instance and take the internal lock.
     * Internal lock is released on destruction.
     */
    ThreadHangStatsIterator();

    /**
     * Get the next item in the list; the first call returns the first item.
     * Returns nullptr at the end of the list.
     */
    Telemetry::ThreadHangStats* GetNext();
  };

  /**
   * Enable hang monitoring.
   * Must return before using BackgroundHangMonitor.
   */
  static void Startup();

  /**
   * Disable hang monitoring.
   * Can be called without destroying all BackgroundHangMonitors first.
   */
  static void Shutdown();

  /**
   * Start monitoring hangs for the current thread.
   *
   * @param aName Name to identify the thread with
   * @param aTimeoutMs Amount of time in milliseconds without
   *  activity before registering a hang
   * @param aMaxTimeoutMs Amount of time in milliseconds without
   *  activity before registering a permanent hang
   */
  BackgroundHangMonitor(const char* aName,
                        uint32_t aTimeoutMs,
                        uint32_t aMaxTimeoutMs);

  /**
   * Monitor hangs using an existing monitor
   * associated with the current thread.
   */
  BackgroundHangMonitor();

  /**
   * Destroys the hang monitor; hang monitoring for a thread stops
   * when all monitors associated with the thread are destroyed.
   */
  ~BackgroundHangMonitor();

  /**
   * Notify the hang monitor of pending current thread activity.
   * Call this method before starting an "activity" or after
   * exiting from a wait state.
   */
  void NotifyActivity();

  /**
   * Notify the hang monitor of current thread wait.
   * Call this method before entering a wait state; call
   * NotifyActivity when subsequently exiting the wait state.
   */
  void NotifyWait();
};

} // namespace mozilla

#endif // mozilla_BackgroundHangMonitor_h
