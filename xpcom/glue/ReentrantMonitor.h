/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ReentrantMonitor_h
#define mozilla_ReentrantMonitor_h

#include "prmon.h"

#ifdef MOZILLA_INTERNAL_API
#include "GeckoProfiler.h"
#endif //MOZILLA_INTERNAL_API

#include "mozilla/BlockingResourceBase.h"

//
// Provides:
//
//  - ReentrantMonitor, a Java-like monitor
//  - ReentrantMonitorAutoEnter, an RAII class for ensuring that
//    ReentrantMonitors are properly entered and exited
//
// Using ReentrantMonitorAutoEnter is MUCH preferred to making bare calls to
// ReentrantMonitor.Enter and Exit.
//
namespace mozilla {


/**
 * ReentrantMonitor
 * Java-like monitor.
 * When possible, use ReentrantMonitorAutoEnter to hold this monitor within a
 * scope, instead of calling Enter/Exit directly.
 **/
class NS_COM_GLUE ReentrantMonitor : BlockingResourceBase
{
public:
  /**
   * ReentrantMonitor
   * @param aName A name which can reference this monitor
   */
  explicit ReentrantMonitor(const char* aName)
    : BlockingResourceBase(aName, eReentrantMonitor)
#ifdef DEBUG
    , mEntryCount(0)
#endif
  {
    MOZ_COUNT_CTOR(ReentrantMonitor);
    mReentrantMonitor = PR_NewMonitor();
    if (!mReentrantMonitor) {
      NS_RUNTIMEABORT("Can't allocate mozilla::ReentrantMonitor");
    }
  }

  /**
   * ~ReentrantMonitor
   **/
  ~ReentrantMonitor()
  {
    NS_ASSERTION(mReentrantMonitor,
                 "improperly constructed ReentrantMonitor or double free");
    PR_DestroyMonitor(mReentrantMonitor);
    mReentrantMonitor = 0;
    MOZ_COUNT_DTOR(ReentrantMonitor);
  }

#ifndef DEBUG
  /**
   * Enter
   * @see prmon.h
   **/
  void Enter() { PR_EnterMonitor(mReentrantMonitor); }

  /**
   * Exit
   * @see prmon.h
   **/
  void Exit() { PR_ExitMonitor(mReentrantMonitor); }

  /**
   * Wait
   * @see prmon.h
   **/
  nsresult Wait(PRIntervalTime aInterval = PR_INTERVAL_NO_TIMEOUT)
  {
#ifdef MOZILLA_INTERNAL_API
    GeckoProfilerSleepRAII profiler_sleep;
#endif //MOZILLA_INTERNAL_API
    return PR_Wait(mReentrantMonitor, aInterval) == PR_SUCCESS ?
      NS_OK : NS_ERROR_FAILURE;
  }

#else // ifndef DEBUG
  void Enter();
  void Exit();
  nsresult Wait(PRIntervalTime aInterval = PR_INTERVAL_NO_TIMEOUT);

#endif  // ifndef DEBUG

  /**
   * Notify
   * @see prmon.h
   **/
  nsresult Notify()
  {
    return PR_Notify(mReentrantMonitor) == PR_SUCCESS ? NS_OK :
                                                        NS_ERROR_FAILURE;
  }

  /**
   * NotifyAll
   * @see prmon.h
   **/
  nsresult NotifyAll()
  {
    return PR_NotifyAll(mReentrantMonitor) == PR_SUCCESS ? NS_OK :
                                                           NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  /**
   * AssertCurrentThreadIn
   * @see prmon.h
   **/
  void AssertCurrentThreadIn()
  {
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mReentrantMonitor);
  }

  /**
   * AssertNotCurrentThreadIn
   * @see prmon.h
   **/
  void AssertNotCurrentThreadIn()
  {
    // FIXME bug 476536
  }

#else
  void AssertCurrentThreadIn() {}
  void AssertNotCurrentThreadIn() {}

#endif  // ifdef DEBUG

private:
  ReentrantMonitor();
  ReentrantMonitor(const ReentrantMonitor&);
  ReentrantMonitor& operator=(const ReentrantMonitor&);

  PRMonitor* mReentrantMonitor;
#ifdef DEBUG
  int32_t mEntryCount;
#endif
};


/**
 * ReentrantMonitorAutoEnter
 * Enters the ReentrantMonitor when it enters scope, and exits it when
 * it leaves scope.
 *
 * MUCH PREFERRED to bare calls to ReentrantMonitor.Enter and Exit.
 */
class NS_COM_GLUE MOZ_STACK_CLASS ReentrantMonitorAutoEnter
{
public:
  /**
   * Constructor
   * The constructor aquires the given lock.  The destructor
   * releases the lock.
   *
   * @param aReentrantMonitor A valid mozilla::ReentrantMonitor*.
   **/
  explicit ReentrantMonitorAutoEnter(mozilla::ReentrantMonitor& aReentrantMonitor)
    : mReentrantMonitor(&aReentrantMonitor)
  {
    NS_ASSERTION(mReentrantMonitor, "null monitor");
    mReentrantMonitor->Enter();
  }

  ~ReentrantMonitorAutoEnter(void)
  {
    mReentrantMonitor->Exit();
  }

  nsresult Wait(PRIntervalTime aInterval = PR_INTERVAL_NO_TIMEOUT)
  {
    return mReentrantMonitor->Wait(aInterval);
  }

  nsresult Notify() { return mReentrantMonitor->Notify(); }
  nsresult NotifyAll() { return mReentrantMonitor->NotifyAll(); }

private:
  ReentrantMonitorAutoEnter();
  ReentrantMonitorAutoEnter(const ReentrantMonitorAutoEnter&);
  ReentrantMonitorAutoEnter& operator=(const ReentrantMonitorAutoEnter&);
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);

  mozilla::ReentrantMonitor* mReentrantMonitor;
};

/**
 * ReentrantMonitorAutoExit
 * Exit the ReentrantMonitor when it enters scope, and enters it when it leaves
 * scope.
 *
 * MUCH PREFERRED to bare calls to ReentrantMonitor.Exit and Enter.
 */
class MOZ_STACK_CLASS ReentrantMonitorAutoExit
{
public:
  /**
   * Constructor
   * The constructor releases the given lock.  The destructor
   * acquires the lock. The lock must be held before constructing
   * this object!
   *
   * @param aReentrantMonitor A valid mozilla::ReentrantMonitor*. It
   *                 must be already locked.
   **/
  explicit ReentrantMonitorAutoExit(ReentrantMonitor& aReentrantMonitor)
    : mReentrantMonitor(&aReentrantMonitor)
  {
    NS_ASSERTION(mReentrantMonitor, "null monitor");
    mReentrantMonitor->AssertCurrentThreadIn();
    mReentrantMonitor->Exit();
  }

  ~ReentrantMonitorAutoExit(void)
  {
    mReentrantMonitor->Enter();
  }

private:
  ReentrantMonitorAutoExit();
  ReentrantMonitorAutoExit(const ReentrantMonitorAutoExit&);
  ReentrantMonitorAutoExit& operator=(const ReentrantMonitorAutoExit&);
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);

  ReentrantMonitor* mReentrantMonitor;
};

} // namespace mozilla


#endif // ifndef mozilla_ReentrantMonitor_h
