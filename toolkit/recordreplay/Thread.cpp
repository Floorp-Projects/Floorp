/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Thread.h"

#include "ipc/ChildIPC.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "ChunkAllocator.h"
#include "ProcessRewind.h"
#include "SpinLock.h"
#include "ThreadSnapshot.h"

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Thread Organization
///////////////////////////////////////////////////////////////////////////////

/* static */
Monitor* Thread::gMonitor;

/* static */
bool Thread::CurrentIsMainThread() {
  Thread* thread = Current();
  return thread && thread->IsMainThread();
}

void Thread::BindToCurrent() {
  pthread_t self = DirectCurrentThread();
  size_t size = pthread_get_stacksize_np(self);
  uint8_t* base = (uint8_t*)pthread_get_stackaddr_np(self) - size;

  if (IsMainThread()) {
    mStackBase = base;
    mStackSize = size;
  } else {
    MOZ_RELEASE_ASSERT(base == mStackBase);
    MOZ_RELEASE_ASSERT(size == mStackSize);
  }

  if (!IsMainThread() && !mMachId) {
    MOZ_RELEASE_ASSERT(this == Current());
    MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

    mMachId = RecordReplayValue(IsRecording() ? mach_thread_self() : 0);
  }
}

// All threads, indexed by the thread ID.
static Thread* gThreads;

/* static */
Thread* Thread::GetById(size_t aId) {
  MOZ_RELEASE_ASSERT(aId);
  MOZ_RELEASE_ASSERT(aId <= MaxThreadId);
  return &gThreads[aId];
}

/* static */
Thread* Thread::GetByNativeId(NativeThreadId aNativeId) {
  for (size_t id = MainThreadId; id <= MaxThreadId; id++) {
    Thread* thread = GetById(id);
    if (thread->mNativeId == aNativeId) {
      return thread;
    }
  }
  return nullptr;
}

static uint8_t* gThreadStackMemory = nullptr;

static const size_t ThreadStackSize = 2 * 1024 * 1024;

/* static */
Thread* Thread::Current() {
  MOZ_ASSERT(IsRecordingOrReplaying());

  if (!gThreads) {
    return nullptr;
  }

  uint8_t* ptr = (uint8_t*)&ptr;
  Thread* mainThread = GetById(MainThreadId);
  if (MemoryContains(mainThread->mStackBase, mainThread->mStackSize, ptr)) {
    return mainThread;
  }

  if (ptr >= gThreadStackMemory) {
    size_t id = MainThreadId + 1 + (ptr - gThreadStackMemory) / ThreadStackSize;
    if (id <= MaxThreadId) {
      return GetById(id);
    }
  }

  return nullptr;
}

static int gWaitForeverFd;

/* static */
void Thread::InitializeThreads() {
  FileHandle writeFd, readFd;
  DirectCreatePipe(&writeFd, &readFd);
  gWaitForeverFd = readFd;

  gThreads = new Thread[MaxThreadId + 1];

  size_t nbytes = (MaxThreadId - MainThreadId) * ThreadStackSize;
  gThreadStackMemory = (uint8_t*) DirectAllocateMemory(nbytes);

  for (size_t i = MainThreadId; i <= MaxThreadId; i++) {
    Thread* thread = &gThreads[i];
    PodZero(thread);
    new (thread) Thread();

    thread->mId = i;
    thread->mEvents = gRecording->OpenStream(StreamName::Event, i);

    if (i == MainThreadId) {
      thread->BindToCurrent();
      thread->mNativeId = DirectCurrentThread();
    } else {
      thread->mStackBase = gThreadStackMemory + (i - MainThreadId - 1) * ThreadStackSize;
      thread->mStackSize = ThreadStackSize - PageSize * 2;

      // Make some memory between thread stacks inaccessible so that breakpad
      // can tell the different thread stacks apart.
      DirectMakeInaccessible(thread->mStackBase + ThreadStackSize - PageSize,
                             PageSize);

      thread->SetPassThrough(true);
    }

    DirectCreatePipe(&thread->mNotifyfd, &thread->mIdlefd);
  }
}

/* static */
void Thread::ThreadMain(void* aArgument) {
  MOZ_ASSERT(IsRecordingOrReplaying());

  Thread* thread = (Thread*)aArgument;
  MOZ_ASSERT(thread->mId > MainThreadId);

  // mMachId is set in BindToCurrent, which already ran if we forked and then
  // respawned this thread.
  bool forked = !!thread->mMachId;

  thread->SetPassThrough(false);
  thread->BindToCurrent();

  if (forked) {
    AutoPassThroughThreadEvents pt;
    RestoreThreadStack(thread->Id());
  }

  while (true) {
    // Wait until this thread has been given a start routine.
    while (true) {
      {
        MonitorAutoLock lock(*gMonitor);
        if (thread->mStart) {
          break;
        }
      }
      Wait();
    }

    thread->mStart(thread->mStartArg);

    MonitorAutoLock lock(*gMonitor);

    // Clear the start routine to indicate to other threads that this one has
    // finished executing.
    thread->mStart = nullptr;
    thread->mStartArg = nullptr;

    // Notify any other thread waiting for this to finish in JoinThread.
    gMonitor->NotifyAll();
  }
}

/* static */
void Thread::SpawnAllThreads() {
  MOZ_ASSERT(AreThreadEventsPassedThrough());

  InitializeThreadSnapshots();

  gMonitor = new Monitor();

  // All Threads are spawned up front. This allows threads to be scanned
  // (e.g. in ReplayUnlock) without worrying about racing with other threads
  // being spawned.
  for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
    // mNativeId reflects the ID when the original process started, ignoring
    // any IDs of threads that are respawned after forking.
    Thread* thread = GetById(i);
    thread->mNativeId = SpawnThread(thread);
  }
}

/* static */
void Thread::SpawnNonRecordedThread(Callback aStart, void* aArgument) {
  DirectSpawnThread(aStart, aArgument, nullptr, 0);
}

/* static */
void Thread::RespawnAllThreadsAfterFork() {
  MOZ_ASSERT(AreThreadEventsPassedThrough());
  for (size_t id = MainThreadId; id <= MaxThreadId; id++) {
    Thread* thread = GetById(id);
    DirectCloseFile(thread->mNotifyfd);
    DirectCloseFile(thread->mIdlefd);
    DirectCreatePipe(&thread->mNotifyfd, &thread->mIdlefd);
    if (!thread->IsMainThread()) {
      SaveThreadStack(id);
      SpawnThread(thread);
    }
  }
}

/* static */
NativeThreadId Thread::SpawnThread(Thread* aThread) {
  return DirectSpawnThread(ThreadMain, aThread, aThread->mStackBase,
                           aThread->mStackSize);
}

/* static */
NativeThreadId Thread::StartThread(Callback aStart, void* aArgument,
                                   bool aNeedsJoin) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return 0;
  }

  MonitorAutoLock lock(*gMonitor);

  size_t id = 0;
  if (IsRecording()) {
    // Look for an idle thread.
    for (id = MainThreadId + 1; id <= MaxThreadId; id++) {
      Thread* targetThread = Thread::GetById(id);
      if (!targetThread->mStart && !targetThread->mNeedsJoin) {
        break;
      }
    }
    if (id > MaxThreadId) {
      child::ReportFatalError("Too many threads");
    }
    MOZ_RELEASE_ASSERT(id <= MaxThreadId);
  }
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CreateThread);
  thread->Events().RecordOrReplayScalar(&id);

  Thread* targetThread = GetById(id);

  // Block until the thread is ready for a new start routine.
  while (targetThread->mStart) {
    MOZ_RELEASE_ASSERT(IsReplaying());
    gMonitor->Wait();
  }

  targetThread->mStart = aStart;
  targetThread->mStartArg = aArgument;
  targetThread->mNeedsJoin = aNeedsJoin;

  // Notify the thread in case it is waiting for a start routine under
  // ThreadMain.
  Notify(id);

  return targetThread->mNativeId;
}

void Thread::Join() {
  MOZ_ASSERT(!AreThreadEventsPassedThrough());

  EnsureNotDivergedFromRecording(Nothing());

  while (true) {
    MonitorAutoLock lock(*gMonitor);
    if (!mStart) {
      MOZ_RELEASE_ASSERT(mNeedsJoin);
      mNeedsJoin = false;
      break;
    }
    gMonitor->Wait();
  }
}

void Thread::AddOwnedLock(NativeLock* aNativeLock) {
  mOwnedLocks.append(aNativeLock);
}

void Thread::RemoveOwnedLock(NativeLock* aNativeLock) {
  for (int i = mOwnedLocks.length() - 1; i >= 0; i--) {
    if (mOwnedLocks[i] == aNativeLock) {
      mOwnedLocks.erase(&mOwnedLocks[i]);
      return;
    }
  }
  MOZ_CRASH("RemoveOwnedLock");
}

void Thread::ReleaseOrAcquireOwnedLocks(OwnedLockState aState) {
  MOZ_RELEASE_ASSERT(aState != OwnedLockState::None);
  for (NativeLock* lock : mOwnedLocks) {
    if (aState == OwnedLockState::NeedRelease) {
      DirectUnlockMutex(lock, /* aPassThroughEvents */ false);
    } else {
      DirectLockMutex(lock, /* aPassThroughEvents */ false);
    }
  }
}

void** Thread::GetOrCreateStorage(uintptr_t aKey) {
  for (StorageEntry** pentry = &mStorageEntries; *pentry; pentry = &(*pentry)->mNext) {
    StorageEntry* entry = *pentry;
    if (entry->mKey == aKey) {
      // Put this at the front of the list.
      *pentry = entry->mNext;
      entry->mNext = mStorageEntries;
      mStorageEntries = entry;
      return &entry->mData;
    }
  }
  StorageEntry* entry = (StorageEntry*) AllocateStorage(sizeof(StorageEntry));
  entry->mKey = aKey;
  entry->mData = 0;
  entry->mNext = mStorageEntries;
  mStorageEntries = entry;
  return &entry->mData;
}

uint8_t* Thread::AllocateStorage(size_t aSize) {
  // malloc uses TLS, so go directly to the system to allocate TLS storage.
  if (mStorageCursor + aSize >= mStorageLimit) {
    size_t nbytes = std::max(aSize, PageSize);
    mStorageCursor = (uint8_t*) DirectAllocateMemory(nbytes);
    mStorageLimit = mStorageCursor + nbytes;
  }
  uint8_t* res = mStorageCursor;
  mStorageCursor += aSize;
  return res;
}

///////////////////////////////////////////////////////////////////////////////
// Thread Public API Accessors
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalBeginPassThroughThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    Thread::Current()->SetPassThrough(true);
  }
}

MOZ_EXPORT void RecordReplayInterface_InternalEndPassThroughThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    Thread::Current()->SetPassThrough(false);
  }
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsPassedThrough() {
  MOZ_ASSERT(IsRecordingOrReplaying());

  // If initialization fails, pass through all thread events until we're able
  // to report the problem to the middleman and die.
  if (gInitializationFailureMessage) {
    return true;
  }

  Thread* thread = Thread::Current();
  return !thread || thread->PassThroughEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalBeginDisallowThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread::Current()->BeginDisallowEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalEndDisallowThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread::Current()->EndDisallowEvents();
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsDisallowed() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread* thread = Thread::Current();
  return thread && thread->AreEventsDisallowed();
}

}  // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Thread Coordination
///////////////////////////////////////////////////////////////////////////////

/* static */
void Thread::WaitForIdleThreads() {
  MOZ_RELEASE_ASSERT(CurrentIsMainThread());

  MonitorAutoLock lock(*gMonitor);
  for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
    Thread* thread = GetById(i);
    thread->mShouldIdle = true;
    thread->mUnrecordedWaitNotified = false;
  }
  while (true) {
    bool done = true;
    for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
      Thread* thread = GetById(i);
      if (!thread->mIdle) {
        done = false;

        // Check if there is a callback we can invoke to get this thread to
        // make progress. The mUnrecordedWaitOnlyWhenDiverged flag is used to
        // avoid perturbing the behavior of threads that may or may not be
        // waiting on an unrecorded resource, depending on whether they have
        // diverged from the recording yet.
        if (thread->mUnrecordedWaitCallback &&
            !thread->mUnrecordedWaitNotified) {
          // Set this flag before releasing the idle lock. Otherwise it's
          // possible the thread could call NotifyUnrecordedWait while we
          // aren't holding the lock, and we would set the flag afterwards
          // without first invoking the callback.
          thread->mUnrecordedWaitNotified = true;

          // Release the idle lock here to avoid any risk of deadlock.
          std::function<void()> callback = thread->mUnrecordedWaitCallback;
          {
            MonitorAutoUnlock unlock(*gMonitor);
            AutoPassThroughThreadEvents pt;
            callback();
          }

          // Releasing the global lock means that we need to start over
          // checking whether there are any idle threads. By marking this
          // thread as having been notified we have made progress, however.
          done = true;
          i = MainThreadId;
        }
      }
    }
    if (done) {
      break;
    }
    MonitorAutoUnlock unlock(*gMonitor);
    WaitNoIdle();
  }
}

/* static */
void Thread::OperateOnIdleThreadLocks(OwnedLockState aState) {
  MOZ_RELEASE_ASSERT(CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(aState != OwnedLockState::None);
  for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
    Thread* thread = GetById(i);
    if (thread->mOwnedLocks.length()) {
      thread->mOwnedLockState = aState;
      Notify(i);
      while (thread->mOwnedLockState != OwnedLockState::None) {
        WaitNoIdle();
      }
    }
  }
}

/* static */
void Thread::ResumeIdleThreads() {
  MOZ_RELEASE_ASSERT(CurrentIsMainThread());
  for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
    GetById(i)->mShouldIdle = false;
    Notify(i);
  }
}

void Thread::NotifyUnrecordedWait(
    const std::function<void()>& aNotifyCallback) {
  if (IsMainThread()) {
    return;
  }

  MonitorAutoLock lock(*gMonitor);
  if (mUnrecordedWaitCallback) {
    // Per the documentation for NotifyUnrecordedWait, we need to call the
    // routine after a notify, even if the routine has been called already
    // since the main thread started to wait for idle replay threads.
    mUnrecordedWaitNotified = false;
  } else {
    MOZ_RELEASE_ASSERT(!mUnrecordedWaitNotified);
  }

  mUnrecordedWaitCallback = aNotifyCallback;

  // The main thread might be able to make progress now by calling the routine
  // if it is waiting for idle replay threads.
  if (mShouldIdle) {
    Notify(MainThreadId);
  }
}

bool Thread::MaybeWaitForFork(
    const std::function<void()>& aReleaseCallback) {
  MOZ_RELEASE_ASSERT(!PassThroughEvents());
  if (IsMainThread()) {
    return false;
  }
  MonitorAutoLock lock(*gMonitor);
  if (!mShouldIdle) {
    return false;
  }
  aReleaseCallback();
  while (mShouldIdle) {
    MonitorAutoUnlock unlock(*gMonitor);
    Wait();
  }
  return true;
}

/* static */
void Thread::WaitNoIdle() {
  Thread* thread = Current();
  uint8_t data = 0;
  size_t read = DirectRead(thread->mIdlefd, &data, 1);
  MOZ_RELEASE_ASSERT(read == 1);
}

/* static */
void Thread::Wait() {
  Thread* thread = Current();

  if (thread->IsMainThread()) {
    WaitNoIdle();
    return;
  }

  // The state saved for a thread needs to match up with the most recent
  // point at which it became idle, so that when the main thread saves the
  // stacks from all threads it saves those stacks at the right point.
  // SaveThreadState might trigger thread events, so make sure they are
  // passed through.
  thread->SetPassThrough(true);
  int stackSeparator = 0;
  if (!SaveThreadState(thread->Id(), &stackSeparator)) {
    // We just restored the stack, notify the main thread since it is waiting
    // for all threads to restore their stacks.
    Notify(MainThreadId);
  }

  thread->mIdle = true;
  if (thread->mShouldIdle) {
    // Notify the main thread that we just became idle.
    Notify(MainThreadId);
  }

  do {
    // Release or reacquire owned locks if the main thread asked us to.
    if (thread->mOwnedLockState != OwnedLockState::None) {
      thread->ReleaseOrAcquireOwnedLocks(thread->mOwnedLockState);
      thread->mOwnedLockState = OwnedLockState::None;
      Notify(MainThreadId);
    }

    // Do the actual waiting for another thread to notify this one.
    WaitNoIdle();
  } while (thread->mShouldIdle);

  thread->mIdle = false;
  thread->SetPassThrough(false);
}

/* static */
void Thread::WaitForever() {
  while (true) {
    Wait();
  }
  Unreachable();
}

/* static */
void Thread::WaitForeverNoIdle() {
  while (true) {
    uint8_t data;
    DirectRead(gWaitForeverFd, &data, 1);
  }
}

/* static */
void Thread::Notify(size_t aId) {
  uint8_t data = 0;
  DirectWrite(GetById(aId)->mNotifyfd, &data, 1);
}

/* static */
size_t Thread::TotalEventProgress() {
  size_t result = 0;
  for (size_t id = MainThreadId; id <= MaxThreadId; id++) {
    Thread* thread = GetById(id);

    // Accessing the stream position here is racy. The returned value is used to
    // determine if a process is making progress over a long period of time
    // (several seconds) and absolute accuracy is not necessary.
    result += thread->mEvents->StreamPosition();
  }
  return result;
}

}  // namespace recordreplay
}  // namespace mozilla
