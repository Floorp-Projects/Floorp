/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Thread_h
#define mozilla_recordreplay_Thread_h

#include "mozilla/Atomics.h"
#include "File.h"
#include "Lock.h"
#include "Monitor.h"

#include <pthread.h>
#include <setjmp.h>

namespace mozilla {
namespace recordreplay {

// Threads Overview.
//
// The main thread and each thread that is spawned when thread events are not
// passed through have their behavior recorded.
//
// While recording, each recorded thread has an associated Thread object which
// can be fetched with Thread::Current and stores the thread's ID, its file for
// storing events that occur in the thread, and some other thread local state.
// Otherwise, threads are spawned and destroyed as usual for the process.
//
// While rewinding, the same Thread structure exists for each recorded thread.
// Several additional changes are needed to facilitate rewinding and IPC:
//
// 1. All recorded threads are spawned early on, before any checkpoint has been
//    reached. These threads idle until the process calls the system's thread
//    creation API, and then they run with the start routine the process
//    provided. After the start routine finishes they idle indefinitely,
//    potentially running new start routines if their thread ID is reused. This
//    allows the process to rewind itself without needing to spawn or destroy
//    any threads.
//
// 2. Some additional number of threads are spawned for use by the IPC and
//    memory snapshot mechanisms. These have associated Thread
//    structures but are not recorded and always pass through thread events.
//
// 3. All recorded threads and must be able to enter a particular blocking
//    state, under Thread::Wait, when requested by the main thread calling
//    WaitForIdleThreads. For most recorded threads this happens when the
//    thread attempts to take a recorded lock and blocks in Lock::Wait.
//    The only exception is for JS helper threads, which never take recorded
//    locks. For these threads, NotifyUnrecordedWait and
//    MaybeWaitForCheckpointSave must be used to enter this state.
//
// 4. Once all recorded threads are idle, the main thread is able to record
//    memory snapshots and thread stacks for later rewinding. Additional
//    threads created for #2 above do not idle and do not have their state
//    included in snapshots, but they are designed to avoid interfering with
//    the main thread while it is taking or restoring a checkpoint.

// The ID used by the process main thread.
static const size_t MainThreadId = 1;

// The maximum ID useable by recorded threads.
static const size_t MaxRecordedThreadId = 70;

// The maximum number of threads which are not recorded but need a Thread so
// that they can participate in e.g. Wait/Notify calls.
static const size_t MaxNumNonRecordedThreads = 12;

static const size_t MaxThreadId = MaxRecordedThreadId + MaxNumNonRecordedThreads;

typedef pthread_t NativeThreadId;

// Information about the execution state of a thread.
class Thread
{
public:
  // Signature for the start function of a thread.
  typedef void (*Callback)(void*);

  // Number of recent assertions remembered.
  static const size_t NumRecentAsserts = 128;

  struct RecentAssertInfo {
    char* mText;
    size_t mPosition;
  };

private:
  // Monitor used to protect various thread information (see Thread.h) and to
  // wait on or signal progress for a thread.
  static Monitor* gMonitor;

  // Thread ID in the recording, fixed at creation.
  size_t mId;

  // Whether to pass events in the thread through without recording/replaying.
  // This is only used by the associated thread.
  bool mPassThroughEvents;

  // Whether to crash if we try to record/replay thread events. This is only
  // used by the associated thread.
  size_t mDisallowEvents;

  // Whether to capture stack information for events while recording. This is
  // only used by the associated thread.
  size_t mCaptureEventStacks;

  // Start routine and argument which the thread is currently executing. This
  // is cleared after the routine finishes and another start routine may be
  // assigned to the thread. mNeedsJoin specifies whether the thread must be
  // joined before it is completely dead and can be reused. This is protected
  // by the thread monitor.
  Callback mStart;
  void* mStartArg;
  bool mNeedsJoin;

  // ID for this thread used by the system.
  NativeThreadId mNativeId;

  // Streams with events and assertions for the thread. These are only used by
  // the associated thread.
  Stream* mEvents;
  Stream* mAsserts;

  // Recent assertions that have been encountered, for debugging.
  RecentAssertInfo mRecentAsserts[NumRecentAsserts];

  // Buffer for general use. This is only used by the associated thread.
  char* mBuffer;
  size_t mBufferCapacity;

  // Stack boundary of the thread, protected by the thread monitor.
  uint8_t* mStackBase;
  size_t mStackSize;

  // File descriptor to block on when the thread is idle, fixed at creation.
  FileHandle mIdlefd;

  // File descriptor to notify to wake the thread up, fixed at creation.
  FileHandle mNotifyfd;

  // Whether the thread is waiting on idlefd.
  Atomic<bool, SequentiallyConsistent, Behavior::DontPreserve> mIdle;

  // Any callback which should be invoked so the thread can make progress,
  // and whether the callback has been invoked yet while the main thread is
  // waiting for threads to become idle. Protected by the thread monitor.
  std::function<void()> mUnrecordedWaitCallback;
  bool mUnrecordedWaitNotified;

public:
///////////////////////////////////////////////////////////////////////////////
// Public Routines
///////////////////////////////////////////////////////////////////////////////

  // Accessors for some members that never change.
  size_t Id() { return mId; }
  NativeThreadId NativeId() { return mNativeId; }
  Stream& Events() { return *mEvents; }
  Stream& Asserts() { return *mAsserts; }
  uint8_t* StackBase() { return mStackBase; }
  size_t StackSize() { return mStackSize; }

  inline bool IsMainThread() { return mId == MainThreadId; }
  inline bool IsRecordedThread() { return mId <= MaxRecordedThreadId; }
  inline bool IsNonMainRecordedThread() { return IsRecordedThread() && !IsMainThread(); }

  // Access the flag for whether this thread is passing events through.
  void SetPassThrough(bool aPassThrough) {
    MOZ_RELEASE_ASSERT(mPassThroughEvents == !aPassThrough);
    mPassThroughEvents = aPassThrough;
  }
  bool PassThroughEvents() {
    return mPassThroughEvents;
  }

  // Access the counter for whether events are disallowed in this thread.
  void BeginDisallowEvents() {
    mDisallowEvents++;
  }
  void EndDisallowEvents() {
    MOZ_RELEASE_ASSERT(mDisallowEvents);
    mDisallowEvents--;
  }
  bool AreEventsDisallowed() {
    return mDisallowEvents != 0;
  }

  // Access the counter for whether event stacks are captured while recording.
  void BeginCaptureEventStacks() {
    mCaptureEventStacks++;
  }
  void EndCaptureEventStacks() {
    MOZ_RELEASE_ASSERT(mCaptureEventStacks);
    mCaptureEventStacks--;
  }
  bool ShouldCaptureEventStacks() {
    return mCaptureEventStacks != 0;
  }

  // Access the array of recent assertions in the thread.
  RecentAssertInfo& RecentAssert(size_t i) {
    MOZ_ASSERT(i < NumRecentAsserts);
    return mRecentAsserts[i];
  }

  // Access a thread local buffer of a guaranteed size. The buffer must be
  // restored before it can be taken again.
  char* TakeBuffer(size_t aSize);
  void RestoreBuffer(char* aBuf);

  // The actual start routine at the root of all recorded threads, and of all
  // threads when replaying.
  static void ThreadMain(void* aArgument);

  // Bind this Thread to the current system thread, setting Thread::Current()
  // and some other basic state.
  void BindToCurrent();

  // Initialize thread state.
  static void InitializeThreads();

  // Get the current thread, or null if this is a system thread.
  static Thread* Current();

  // Helper to test if this is the process main thread.
  static bool CurrentIsMainThread();

  // Lookup a Thread by various methods.
  static Thread* GetById(size_t aId);
  static Thread* GetByNativeId(NativeThreadId aNativeId);
  static Thread* GetByStackPointer(void* aSp);

  // Spawn all non-main recorded threads used for recording/replaying.
  static void SpawnAllThreads();

  // Spawn the specified thread.
  static void SpawnThread(Thread* aThread);

  // Spawn a non-recorded thread with the specified start routine/argument.
  static Thread* SpawnNonRecordedThread(Callback aStart, void* aArgument);

  // Wait until a thread has initialized its stack and other state.
  static void WaitUntilInitialized(Thread* aThread);

  // Start an existing thread, for use when the process has called a thread
  // creation system API when events were not passed through. The return value
  // is the native ID of the result.
  static NativeThreadId StartThread(Callback aStart, void* aArgument, bool aNeedsJoin);

  // Wait until this thread finishes executing its start routine.
  void Join();

///////////////////////////////////////////////////////////////////////////////
// Thread Coordination
///////////////////////////////////////////////////////////////////////////////

  // Basic API for threads to coordinate activity with each other, for use
  // during replay. Each Notify() on a thread ID will cause that thread to
  // return from one call to Wait(). Thus, if a thread Wait()'s and then
  // another thread Notify()'s its ID, the first thread will wake up afterward.
  // Similarly, if a thread Notify()'s another thread which is not waiting,
  // that second thread will return from its next Wait() without needing
  // another Notify().
  //
  // If the main thread has called WaitForIdleThreads, then calling
  // Wait() will put this thread in the desired idle state. WaitNoIdle() will
  // never cause the thread to enter the idle state, and should be used
  // carefully to avoid deadlocks with the main thread.
  static void Wait();
  static void WaitNoIdle();
  static void Notify(size_t aId);

  // Wait indefinitely, until the process is rewound.
  static void WaitForever();

  // Wait indefinitely, without allowing this thread to be rewound.
  static void WaitForeverNoIdle();

  // See RecordReplay.h.
  void NotifyUnrecordedWait(const std::function<void()>& aCallback);
  static void MaybeWaitForCheckpointSave();

  // Wait for all other threads to enter the idle state necessary for saving
  // or restoring a checkpoint. This may only be called on the main thread.
  static void WaitForIdleThreads();

  // After WaitForIdleThreads(), the main thread will call this to allow
  // other threads to resume execution.
  static void ResumeIdleThreads();
};

// This uses a stack pointer instead of TLS to make sure events are passed
// through, for avoiding thorny reentrance issues.
class AutoEnsurePassThroughThreadEventsUseStackPointer
{
  Thread* mThread;
  bool mPassedThrough;

public:
  AutoEnsurePassThroughThreadEventsUseStackPointer()
    : mThread(Thread::GetByStackPointer(this))
    , mPassedThrough(!mThread || mThread->PassThroughEvents())
  {
    if (!mPassedThrough) {
      mThread->SetPassThrough(true);
    }
  }

  ~AutoEnsurePassThroughThreadEventsUseStackPointer()
  {
    if (!mPassedThrough) {
      mThread->SetPassThrough(false);
    }
  }
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_Thread_h
