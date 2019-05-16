/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRedirect.h"

#include "mozilla/Maybe.h"

#include "HashTable.h"
#include "Lock.h"
#include "MemorySnapshot.h"
#include "ProcessRecordReplay.h"
#include "ProcessRewind.h"
#include "base/eintr_wrapper.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>

#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <sys/attr.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <Carbon/Carbon.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <objc/objc-runtime.h>

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Original functions
///////////////////////////////////////////////////////////////////////////////

// Specify all the redirections for which the original function (with its
// normal non-redirected semantics) is needed.
#define FOR_EACH_ORIGINAL_FUNCTION(MACRO)   \
  MACRO(__workq_kernreturn)                 \
  MACRO(CFDataGetLength)                    \
  MACRO(CGPathApply)                        \
  MACRO(close)                              \
  MACRO(lseek)                              \
  MACRO(mach_absolute_time)                 \
  MACRO(mmap)                               \
  MACRO(mprotect)                           \
  MACRO(munmap)                             \
  MACRO(objc_msgSend)                       \
  MACRO(open)                               \
  MACRO(OSSpinLockLock)                     \
  MACRO(pipe)                               \
  MACRO(PL_HashTableDestroy)                \
  MACRO(pthread_cond_wait)                  \
  MACRO(pthread_cond_timedwait)             \
  MACRO(pthread_cond_timedwait_relative_np) \
  MACRO(pthread_create)                     \
  MACRO(pthread_mutex_destroy)              \
  MACRO(pthread_mutex_init)                 \
  MACRO(pthread_mutex_lock)                 \
  MACRO(pthread_mutex_trylock)              \
  MACRO(pthread_mutex_unlock)               \
  MACRO(read)                               \
  MACRO(start_wqthread)                     \
  MACRO(write)

#define DECLARE_ORIGINAL_FUNCTION(aName) static void* gOriginal_##aName;

FOR_EACH_ORIGINAL_FUNCTION(DECLARE_ORIGINAL_FUNCTION)

#undef DECLARE_ORIGINAL_FUNCTION

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////

enum CallbackEvent {
  CallbackEvent_CFRunLoopPerformCallBack,
  CallbackEvent_CGPathApplierFunction
};

typedef void (*CFRunLoopPerformCallBack)(void*);

static void CFRunLoopPerformCallBackWrapper(void* aInfo) {
  RecordReplayCallback(CFRunLoopPerformCallBack, &aInfo);
  rrc.mFunction(aInfo);

  // Make sure we service any callbacks that have been posted for the main
  // thread whenever the main thread's message loop has any activity.
  PauseMainThreadAndServiceCallbacks();
}

static size_t CGPathElementPointCount(CGPathElement* aElement) {
  switch (aElement->type) {
    case kCGPathElementCloseSubpath:
      return 0;
    case kCGPathElementMoveToPoint:
    case kCGPathElementAddLineToPoint:
      return 1;
    case kCGPathElementAddQuadCurveToPoint:
      return 2;
    case kCGPathElementAddCurveToPoint:
      return 3;
    default:
      MOZ_CRASH();
  }
}

static void CGPathApplierFunctionWrapper(void* aInfo, CGPathElement* aElement) {
  RecordReplayCallback(CGPathApplierFunction, &aInfo);

  CGPathElement replayElement;
  if (IsReplaying()) {
    aElement = &replayElement;
  }

  aElement->type = (CGPathElementType)RecordReplayValue(aElement->type);

  size_t npoints = CGPathElementPointCount(aElement);
  if (IsReplaying()) {
    aElement->points = new CGPoint[npoints];
  }
  RecordReplayBytes(aElement->points, npoints * sizeof(CGPoint));

  rrc.mFunction(aInfo, aElement);

  if (IsReplaying()) {
    delete[] aElement->points;
  }
}

void ReplayInvokeCallback(size_t aCallbackId) {
  MOZ_RELEASE_ASSERT(IsReplaying());
  switch (aCallbackId) {
    case CallbackEvent_CFRunLoopPerformCallBack:
      CFRunLoopPerformCallBackWrapper(nullptr);
      break;
    case CallbackEvent_CGPathApplierFunction:
      CGPathApplierFunctionWrapper(nullptr, nullptr);
      break;
    default:
      MOZ_CRASH();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Middleman Call Helpers
///////////////////////////////////////////////////////////////////////////////

// List of the Objective C classes which messages might be sent to directly.
static const char* gStaticClassNames[] = {
    // Standard classes.
    "NSAutoreleasePool",
    "NSBezierPath",
    "NSButtonCell",
    "NSColor",
    "NSComboBoxCell",
    "NSDictionary",
    "NSGraphicsContext",
    "NSFont",
    "NSFontManager",
    "NSLevelIndicatorCell",
    "NSNumber",
    "NSPopUpButtonCell",
    "NSProgressBarCell",
    "NSString",
    "NSWindow",

    // Gecko defined classes.
    "CellDrawView",
    "CheckboxCell",
    "RadioButtonCell",
    "SearchFieldCellWithFocusRing",
    "ToolbarSearchFieldCellWithFocusRing",
};

// Objective C classes for each of the above class names.
static Class* gStaticClasses;

// Inputs that originate from static data in the replaying process itself
// rather than from previous middleman calls.
enum class ObjCInputKind {
  StaticClass,
  ConstantString,
};

// Internal layout of a constant compile time CFStringRef.
struct CFConstantString {
  Class mClass;
  size_t mStuff;
  char* mData;
  size_t mLength;
};

static Class gCFConstantStringClass;

static void InitializeStaticClasses() {
  gStaticClasses = new Class[ArrayLength(gStaticClassNames)];

  for (size_t i = 0; i < ArrayLength(gStaticClassNames); i++) {
    gStaticClasses[i] = objc_lookUpClass(gStaticClassNames[i]);
    MOZ_RELEASE_ASSERT(gStaticClasses[i]);
  }

  gCFConstantStringClass = objc_lookUpClass("__NSCFConstantString");
  MOZ_RELEASE_ASSERT(gCFConstantStringClass);
}

// Capture an Objective C or CoreFoundation input to a call, which may come
// either from another middleman call, or from static data in the replaying
// process.
static void MM_ObjCInput(MiddlemanCallContext& aCx, id* aThingPtr) {
  MOZ_RELEASE_ASSERT(aCx.AccessPreface());

  if (MM_SystemInput(aCx, (const void**)aThingPtr)) {
    // This value came from a previous middleman call.
    return;
  }

  MOZ_RELEASE_ASSERT(aCx.AccessInput());

  if (aCx.mPhase == MiddlemanCallPhase::ReplayInput) {
    // Try to determine where this object came from.

    // Watch for messages sent to particular classes.
    for (size_t i = 0; i < ArrayLength(gStaticClassNames); i++) {
      if (gStaticClasses[i] == (Class)*aThingPtr) {
        const char* className = gStaticClassNames[i];
        aCx.WriteInputScalar((size_t)ObjCInputKind::StaticClass);
        size_t len = strlen(className) + 1;
        aCx.WriteInputScalar(len);
        aCx.WriteInputBytes(className, len);
        return;
      }
    }

    // Watch for constant compile time strings baked into the generated code or
    // stored in system libraries. Be careful when accessing the pointer as in
    // the case where a middleman call hook for a function is missing the
    // pointer could have originated from the recording and its address may not
    // be mapped. In this case we would rather gracefully recover and fail to
    // paint, instead of crashing.
    if (MemoryRangeIsTracked(*aThingPtr, sizeof(CFConstantString))) {
      CFConstantString* str = (CFConstantString*)*aThingPtr;
      if (str->mClass == gCFConstantStringClass &&
          str->mLength <= 4096 &&  // Sanity check.
          MemoryRangeIsTracked(str->mData, str->mLength)) {
        InfallibleVector<UniChar> buffer;
        NS_ConvertUTF8toUTF16 converted(str->mData, str->mLength);
        aCx.WriteInputScalar((size_t)ObjCInputKind::ConstantString);
        aCx.WriteInputScalar(str->mLength);
        aCx.WriteInputBytes(converted.get(), str->mLength * sizeof(UniChar));
        return;
      }
    }

    aCx.MarkAsFailed();
    return;
  }

  switch ((ObjCInputKind)aCx.ReadInputScalar()) {
    case ObjCInputKind::StaticClass: {
      size_t len = aCx.ReadInputScalar();
      UniquePtr<char[]> className(new char[len]);
      aCx.ReadInputBytes(className.get(), len);
      *aThingPtr = (id)objc_lookUpClass(className.get());
      break;
    }
    case ObjCInputKind::ConstantString: {
      size_t len = aCx.ReadInputScalar();
      UniquePtr<UniChar[]> contents(new UniChar[len]);
      aCx.ReadInputBytes(contents.get(), len * sizeof(UniChar));
      *aThingPtr = (id)CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                    contents.get(), len);
      break;
    }
    default:
      MOZ_CRASH();
  }
}

template <size_t Argument>
static void MM_CFTypeArg(MiddlemanCallContext& aCx) {
  if (aCx.AccessPreface()) {
    auto& object = aCx.mArguments->Arg<Argument, id>();
    MM_ObjCInput(aCx, &object);
  }
}

static void MM_CFTypeOutput(MiddlemanCallContext& aCx, CFTypeRef* aOutput,
                            bool aOwnsReference) {
  MM_SystemOutput(aCx, (const void**)aOutput);

  if (*aOutput) {
    switch (aCx.mPhase) {
      case MiddlemanCallPhase::MiddlemanOutput:
        if (!aOwnsReference) {
          CFRetain(*aOutput);
        }
        break;
      case MiddlemanCallPhase::MiddlemanRelease:
        CFRelease(*aOutput);
        break;
      default:
        break;
    }
  }
}

// For APIs using the 'Get' rule: no reference is held on the returned value.
static void MM_CFTypeRval(MiddlemanCallContext& aCx) {
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  MM_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ false);
}

// For APIs using the 'Create' rule: a reference is held on the returned
// value which must be released.
static void MM_CreateCFTypeRval(MiddlemanCallContext& aCx) {
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  MM_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ true);
}

template <size_t Argument>
static void MM_CFTypeOutputArg(MiddlemanCallContext& aCx) {
  MM_WriteBufferFixedSize<Argument, sizeof(const void*)>(aCx);

  auto arg = aCx.mArguments->Arg<Argument, const void**>();
  MM_CFTypeOutput(aCx, arg, /* aOwnsReference = */ false);
}

// For APIs whose result will be released by the middleman's autorelease pool.
static void MM_AutoreleaseCFTypeRval(MiddlemanCallContext& aCx) {
  auto& rval = aCx.mArguments->Rval<const void*>();
  MM_SystemOutput(aCx, &rval);
}

// For functions which have an input CFType value and also have side effects on
// that value, this associates the call with its own input value so that this
// will be treated as a dependent for any future calls using the value.
template <size_t Argument>
static void MM_UpdateCFTypeArg(MiddlemanCallContext& aCx) {
  auto arg = aCx.mArguments->Arg<Argument, const void*>();

  MM_CFTypeArg<Argument>(aCx);
  MM_SystemOutput(aCx, &arg, /* aUpdating = */ true);
}

template <int Error = EAGAIN>
static PreambleResult Preamble_SetError(CallArguments* aArguments) {
  aArguments->Rval<ssize_t>() = -1;
  errno = Error;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// system call redirections
///////////////////////////////////////////////////////////////////////////////

static void RR_recvmsg(Stream& aEvents, CallArguments* aArguments,
                       ErrorType* aError) {
  auto& msg = aArguments->Arg<1, struct msghdr*>();

  aEvents.CheckInput(msg->msg_iovlen);
  for (int i = 0; i < msg->msg_iovlen; i++) {
    aEvents.CheckInput(msg->msg_iov[i].iov_len);
  }

  aEvents.RecordOrReplayValue(&msg->msg_flags);
  aEvents.RecordOrReplayValue(&msg->msg_controllen);
  aEvents.RecordOrReplayBytes(msg->msg_control, msg->msg_controllen);

  size_t nbytes = aArguments->Rval<size_t>();
  for (int i = 0; nbytes && i < msg->msg_iovlen; i++) {
    struct iovec* iov = &msg->msg_iov[i];
    size_t iovbytes = nbytes <= iov->iov_len ? nbytes : iov->iov_len;
    aEvents.RecordOrReplayBytes(iov->iov_base, iovbytes);
    nbytes -= iovbytes;
  }
  MOZ_RELEASE_ASSERT(nbytes == 0);
}

static PreambleResult MiddlemanPreamble_sendmsg(CallArguments* aArguments) {
  // Silently pretend that sends succeed after diverging from the recording.
  size_t totalSize = 0;
  auto msg = aArguments->Arg<1, msghdr*>();
  for (int i = 0; i < msg->msg_iovlen; i++) {
    totalSize += msg->msg_iov[i].iov_len;
  }
  aArguments->Rval<size_t>() = totalSize;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mprotect(CallArguments* aArguments) {
  // Ignore any mprotect calls that occur after saving a checkpoint.
  if (!HasSavedAnyCheckpoint()) {
    return PreambleResult::PassThrough;
  }
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mmap(CallArguments* aArguments) {
  auto& address = aArguments->Arg<0, void*>();
  auto& size = aArguments->Arg<1, size_t>();
  auto& prot = aArguments->Arg<2, size_t>();
  auto& flags = aArguments->Arg<3, size_t>();
  auto& fd = aArguments->Arg<4, size_t>();
  auto& offset = aArguments->Arg<5, size_t>();

  MOZ_RELEASE_ASSERT(address == PageBase(address));

  // Make sure that fixed mappings do not interfere with snapshot state.
  if (flags & MAP_FIXED) {
    CheckFixedMemory(address, RoundupSizeToPageBoundary(size));
  }

  void* memory = nullptr;
  if ((flags & MAP_ANON) ||
      (IsReplaying() && !AreThreadEventsPassedThrough())) {
    // Get an anonymous mapping for the result.
    if (flags & MAP_FIXED) {
      // For fixed allocations, make sure this memory region is mapped and zero.
      if (!HasSavedAnyCheckpoint()) {
        // Make sure this memory region is writable.
        CallFunction<int>(gOriginal_mprotect, address, size,
                          PROT_READ | PROT_WRITE | PROT_EXEC);
      }
      memset(address, 0, size);
      memory = address;
    } else {
      memory = AllocateMemoryTryAddress(
          address, RoundupSizeToPageBoundary(size), MemoryKind::Tracked);
    }
  } else {
    // We have to call mmap itself, which can change memory protection flags
    // for memory that is already allocated. If we haven't saved a checkpoint
    // then this is no problem, but after saving a checkpoint we have to make
    // sure that protection flags are what we expect them to be.
    int newProt = HasSavedAnyCheckpoint() ? (PROT_READ | PROT_EXEC) : prot;
    memory = CallFunction<void*>(gOriginal_mmap, address, size, newProt, flags,
                                 fd, offset);

    if (flags & MAP_FIXED) {
      MOZ_RELEASE_ASSERT(memory == address);
      RestoreWritableFixedMemory(memory, RoundupSizeToPageBoundary(size));
    } else if (memory && memory != (void*)-1) {
      RegisterAllocatedMemory(memory, RoundupSizeToPageBoundary(size),
                              MemoryKind::Tracked);
    }
  }

  if (!(flags & MAP_ANON) && !AreThreadEventsPassedThrough()) {
    // Include the data just mapped in the recording.
    MOZ_RELEASE_ASSERT(memory && memory != (void*)-1);
    RecordReplayBytes(memory, size);
  }

  aArguments->Rval<void*>() = memory;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_munmap(CallArguments* aArguments) {
  auto& address = aArguments->Arg<0, void*>();
  auto& size = aArguments->Arg<1, size_t>();

  DeallocateMemory(address, size, MemoryKind::Tracked);
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult MiddlemanPreamble_write(CallArguments* aArguments) {
  // Silently pretend that writes succeed after diverging from the recording.
  aArguments->Rval<size_t>() = aArguments->Arg<2, size_t>();
  return PreambleResult::Veto;
}

static void RR_getsockopt(Stream& aEvents, CallArguments* aArguments,
                          ErrorType* aError) {
  auto& optval = aArguments->Arg<3, void*>();
  auto& optlen = aArguments->Arg<4, int*>();

  // The initial length has already been clobbered while recording, but make
  // sure there is enough room for the copy while replaying.
  int initLen = *optlen;
  aEvents.RecordOrReplayValue(optlen);
  MOZ_RELEASE_ASSERT(*optlen <= initLen);

  aEvents.RecordOrReplayBytes(optval, *optlen);
}

static PreambleResult Preamble_getpid(CallArguments* aArguments) {
  if (!AreThreadEventsPassedThrough()) {
    // Don't involve the recording with getpid calls, so that this can be used
    // after diverging from the recording.
    aArguments->Rval<size_t>() = GetRecordingPid();
    return PreambleResult::Veto;
  }
  return PreambleResult::Redirect;
}

static PreambleResult Preamble_fcntl(CallArguments* aArguments) {
  // We don't record any outputs for fcntl other than its return value, but
  // some commands have an output parameter they write additional data to.
  // Handle this by only allowing a limited set of commands to be used when
  // events are not passed through and we are recording/replaying the outputs.
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& cmd = aArguments->Arg<1, size_t>();
  switch (cmd) {
    case F_GETFL:
    case F_SETFL:
    case F_GETFD:
    case F_SETFD:
    case F_NOCACHE:
    case F_SETLK:
    case F_SETLKW:
      break;
    default:
      MOZ_CRASH();
  }
  return PreambleResult::Redirect;
}

static PreambleResult MiddlemanPreamble_fcntl(CallArguments* aArguments) {
  auto& cmd = aArguments->Arg<1, size_t>();
  switch (cmd) {
    case F_SETFL:
    case F_SETFD:
    case F_SETLK:
    case F_SETLKW:
      break;
    default:
      MOZ_CRASH();
  }
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult Preamble___disable_threadsignal(
    CallArguments* aArguments) {
  // __disable_threadsignal is called when a thread finishes. During replay a
  // terminated thread can cause problems such as changing access bits on
  // tracked memory behind the scenes.
  //
  // Ideally, threads will never try to finish when we are replaying, since we
  // are supposed to have control over all threads in the system and only spawn
  // threads which will run forever. Unfortunately, GCD might have already
  // spawned threads before we were able to install our redirections, so use a
  // fallback here to keep these threads from terminating.
  if (IsReplaying()) {
    Thread::WaitForeverNoIdle();
  }
  return PreambleResult::PassThrough;
}

static void RR___sysctl(Stream& aEvents, CallArguments* aArguments,
                        ErrorType* aError) {
  auto& old = aArguments->Arg<2, void*>();
  auto& oldlenp = aArguments->Arg<3, size_t*>();

  aEvents.CheckInput((old ? 1 : 0) | (oldlenp ? 2 : 0));
  if (oldlenp) {
    aEvents.RecordOrReplayValue(oldlenp);
  }
  if (old) {
    aEvents.RecordOrReplayBytes(old, *oldlenp);
  }
}

static PreambleResult Preamble___workq_kernreturn(CallArguments* aArguments) {
  // Busy-wait until initialization is complete.
  while (!gInitialized) {
    ThreadYield();
  }

  // Make sure we know this thread exists.
  Thread::Current();

  RecordReplayInvokeCall(gOriginal___workq_kernreturn, aArguments);
  return PreambleResult::Veto;
}

static PreambleResult Preamble_start_wqthread(CallArguments* aArguments) {
  // When replaying we don't want system threads to run, but by the time we
  // initialize the record/replay system GCD has already started running.
  // Use this redirection to watch for new threads being spawned by GCD, and
  // suspend them immediately.
  if (IsReplaying()) {
    Thread::WaitForeverNoIdle();
  }

  RecordReplayInvokeCall(gOriginal_start_wqthread, aArguments);
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// pthreads redirections
///////////////////////////////////////////////////////////////////////////////

static void DirectLockMutex(pthread_mutex_t* aMutex) {
  AutoPassThroughThreadEvents pt;
  ssize_t rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_lock, aMutex);
  MOZ_RELEASE_ASSERT(rv == 0);
}

static void DirectUnlockMutex(pthread_mutex_t* aMutex) {
  AutoPassThroughThreadEvents pt;
  ssize_t rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_unlock, aMutex);
  MOZ_RELEASE_ASSERT(rv == 0);
}

// Handle a redirection which releases a mutex, waits in some way for a cvar,
// and reacquires the mutex before returning.
static ssize_t WaitForCvar(pthread_mutex_t* aMutex, pthread_cond_t* aCond,
                           bool aRecordReturnValue,
                           const std::function<ssize_t()>& aCallback) {
  Lock* lock = Lock::Find(aMutex);
  if (!lock) {
    if (IsReplaying() && !AreThreadEventsPassedThrough()) {
      Thread* thread = Thread::Current();
      if (thread->MaybeWaitForCheckpointSave(
              [=]() { pthread_mutex_unlock(aMutex); })) {
        // We unlocked the mutex while the thread idled, so don't wait on the
        // condvar: the state the thread is waiting on may have changed and it
        // might not want to continue waiting. Returning immediately means this
        // this is a spurious wakeup, which is allowed by the pthreads API and
        // should be handled correctly by callers.
        pthread_mutex_lock(aMutex);
        return 0;
      }
      thread->NotifyUnrecordedWait([=]() {
        pthread_mutex_lock(aMutex);
        pthread_cond_broadcast(aCond);
        pthread_mutex_unlock(aMutex);
      });
    }

    AutoEnsurePassThroughThreadEvents pt;
    return aCallback();
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = aCallback();
  } else {
    DirectUnlockMutex(aMutex);
  }
  lock->Exit();
  lock->Enter();
  if (IsReplaying()) {
    DirectLockMutex(aMutex);
  }
  if (aRecordReturnValue) {
    return RecordReplayValue(rv);
  }
  MOZ_RELEASE_ASSERT(rv == 0);
  return 0;
}

static PreambleResult Preamble_pthread_cond_wait(CallArguments* aArguments) {
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  js::BeginIdleTime();
  aArguments->Rval<ssize_t>() = WaitForCvar(mutex, cond, false, [=]() {
    return CallFunction<ssize_t>(gOriginal_pthread_cond_wait, cond, mutex);
  });
  js::EndIdleTime();
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_cond_timedwait(
    CallArguments* aArguments) {
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  auto& timeout = aArguments->Arg<2, timespec*>();
  aArguments->Rval<ssize_t>() = WaitForCvar(mutex, cond, true, [=]() {
    return CallFunction<ssize_t>(gOriginal_pthread_cond_timedwait, cond, mutex,
                                 timeout);
  });
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_cond_timedwait_relative_np(
    CallArguments* aArguments) {
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  auto& timeout = aArguments->Arg<2, timespec*>();
  aArguments->Rval<ssize_t>() = WaitForCvar(mutex, cond, true, [=]() {
    return CallFunction<ssize_t>(gOriginal_pthread_cond_timedwait_relative_np,
                                 cond, mutex, timeout);
  });
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_create(CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& token = aArguments->Arg<0, pthread_t*>();
  auto& attr = aArguments->Arg<1, const pthread_attr_t*>();
  auto& start = aArguments->Arg<2, void*>();
  auto& startArg = aArguments->Arg<3, void*>();

  int detachState;
  int rv = pthread_attr_getdetachstate(attr, &detachState);
  MOZ_RELEASE_ASSERT(rv == 0);

  *token = Thread::StartThread((Thread::Callback)start, startArg,
                               detachState == PTHREAD_CREATE_JOINABLE);
  if (!*token) {
    // Don't create new threads after diverging from the recording.
    MOZ_RELEASE_ASSERT(HasDivergedFromRecording());
    return Preamble_SetError(aArguments);
  }

  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_join(CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& token = aArguments->Arg<0, pthread_t>();
  auto& ptr = aArguments->Arg<1, void**>();

  Thread* thread = Thread::GetByNativeId(token);
  thread->Join();

  if (ptr) {
    *ptr = nullptr;
  }
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_mutex_init(CallArguments* aArguments) {
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();
  auto& attr = aArguments->Arg<1, pthread_mutexattr_t*>();

  Lock::New(mutex);
  aArguments->Rval<ssize_t>() =
      CallFunction<ssize_t>(gOriginal_pthread_mutex_init, mutex, attr);
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_mutex_destroy(
    CallArguments* aArguments) {
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock::Destroy(mutex);
  aArguments->Rval<ssize_t>() =
      CallFunction<ssize_t>(gOriginal_pthread_mutex_destroy, mutex);
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_mutex_lock(CallArguments* aArguments) {
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEventsUseStackPointer pt;
    aArguments->Rval<ssize_t>() =
        CallFunction<ssize_t>(gOriginal_pthread_mutex_lock, mutex);
    return PreambleResult::Veto;
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_lock, mutex);
  }
  rv = RecordReplayValue(rv);
  MOZ_RELEASE_ASSERT(rv == 0 || rv == EDEADLK);
  if (rv == 0) {
    lock->Enter();
    if (IsReplaying()) {
      DirectLockMutex(mutex);
    }
  }
  aArguments->Rval<ssize_t>() = rv;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_mutex_trylock(
    CallArguments* aArguments) {
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEvents pt;
    aArguments->Rval<ssize_t>() =
        CallFunction<ssize_t>(gOriginal_pthread_mutex_trylock, mutex);
    return PreambleResult::Veto;
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_trylock, mutex);
  }
  rv = RecordReplayValue(rv);
  MOZ_RELEASE_ASSERT(rv == 0 || rv == EBUSY);
  if (rv == 0) {
    lock->Enter();
    if (IsReplaying()) {
      DirectLockMutex(mutex);
    }
  }
  aArguments->Rval<ssize_t>() = rv;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_mutex_unlock(CallArguments* aArguments) {
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEventsUseStackPointer pt;
    aArguments->Rval<ssize_t>() =
        CallFunction<ssize_t>(gOriginal_pthread_mutex_unlock, mutex);
    return PreambleResult::Veto;
  }
  lock->Exit();
  DirectUnlockMutex(mutex);
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// stdlib redirections
///////////////////////////////////////////////////////////////////////////////

static void RR_fread(Stream& aEvents, CallArguments* aArguments,
                     ErrorType* aError) {
  auto& buf = aArguments->Arg<0, void*>();
  auto& elemSize = aArguments->Arg<1, size_t>();
  auto& capacity = aArguments->Arg<2, size_t>();
  auto& rval = aArguments->Rval<size_t>();

  aEvents.CheckInput(elemSize);
  aEvents.CheckInput(capacity);
  MOZ_RELEASE_ASSERT(rval <= capacity);
  aEvents.RecordOrReplayBytes(buf, rval * elemSize);
}

static PreambleResult Preamble_getenv(CallArguments* aArguments) {
  // Ignore attempts to get environment variables that might be fetched in a
  // racy way.
  auto env = aArguments->Arg<0, const char*>();

  // The JPEG library can fetch configuration information from the environment
  // in a way that can run non-deterministically on different threads.
  if (strncmp(env, "JSIMD_", 6) == 0) {
    aArguments->Rval<char*>() = nullptr;
    return PreambleResult::Veto;
  }

  // Include the environment variable being checked in an assertion, to make it
  // easier to debug recording mismatches involving getenv.
  RecordReplayAssert("getenv %s", env);
  return PreambleResult::Redirect;
}

static struct tm gGlobalTM;

// localtime behaves the same as localtime_r, except it is not reentrant.
// For simplicity, define this in terms of localtime_r.
static PreambleResult Preamble_localtime(CallArguments* aArguments) {
  aArguments->Rval<struct tm*>() =
      localtime_r(aArguments->Arg<0, const time_t*>(), &gGlobalTM);
  return PreambleResult::Veto;
}

// The same concern here applies as for localtime.
static PreambleResult Preamble_gmtime(CallArguments* aArguments) {
  aArguments->Rval<struct tm*>() =
      gmtime_r(aArguments->Arg<0, const time_t*>(), &gGlobalTM);
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mach_absolute_time(CallArguments* aArguments) {
  // This function might be called through OSSpinLock while setting
  // gTlsThreadKey.
  Thread* thread = Thread::GetByStackPointer(&thread);
  if (!thread || thread->PassThroughEvents()) {
    aArguments->Rval<uint64_t>() =
        CallFunction<uint64_t>(gOriginal_mach_absolute_time);
    return PreambleResult::Veto;
  }
  return PreambleResult::Redirect;
}

static PreambleResult Preamble_mach_vm_allocate(CallArguments* aArguments) {
  auto& address = aArguments->Arg<1, void**>();
  auto& size = aArguments->Arg<2, size_t>();
  *address = AllocateMemory(size, MemoryKind::Tracked);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mach_vm_deallocate(CallArguments* aArguments) {
  auto& address = aArguments->Arg<1, void*>();
  auto& size = aArguments->Arg<2, size_t>();
  DeallocateMemory(address, size, MemoryKind::Tracked);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mach_vm_map(CallArguments* aArguments) {
  if (IsRecording()) {
    return PreambleResult::PassThrough;
  } else if (AreThreadEventsPassedThrough()) {
    // We should only reach this at startup, when initializing the graphics
    // shared memory block.
    MOZ_RELEASE_ASSERT(!HasSavedAnyCheckpoint());
    return PreambleResult::PassThrough;
  }

  auto size = aArguments->Arg<2, size_t>();
  auto address = aArguments->Arg<1, void**>();

  *address = AllocateMemory(size, MemoryKind::Tracked);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_mach_vm_protect(CallArguments* aArguments) {
  // Ignore any mach_vm_protect calls that occur after saving a checkpoint, as
  // for mprotect.
  if (!HasSavedAnyCheckpoint()) {
    return PreambleResult::PassThrough;
  }
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_vm_purgable_control(CallArguments* aArguments) {
  // Never allow purging of volatile memory, to simplify memory snapshots.
  auto& state = aArguments->Arg<3, int*>();
  *state = VM_PURGABLE_NONVOLATILE;
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_vm_copy(CallArguments* aArguments) {
  // Asking the kernel to copy memory doesn't work right if the destination is
  // non-writable, so do the copy manually.
  auto& src = aArguments->Arg<1, void*>();
  auto& size = aArguments->Arg<2, size_t>();
  auto& dest = aArguments->Arg<3, void*>();
  memcpy(dest, src, size);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// NSPR redirections
///////////////////////////////////////////////////////////////////////////////

// Even though NSPR is compiled as part of firefox, it is easier to just
// redirect this stuff than get record/replay related changes into the source.

static PreambleResult Preamble_PL_NewHashTable(CallArguments* aArguments) {
  auto& keyHash = aArguments->Arg<1, PLHashFunction>();
  auto& keyCompare = aArguments->Arg<2, PLHashComparator>();
  auto& valueCompare = aArguments->Arg<3, PLHashComparator>();
  auto& allocOps = aArguments->Arg<4, const PLHashAllocOps*>();
  auto& allocPriv = aArguments->Arg<5, void*>();

  GeneratePLHashTableCallbacks(&keyHash, &keyCompare, &valueCompare, &allocOps,
                               &allocPriv);
  return PreambleResult::PassThrough;
}

static PreambleResult Preamble_PL_HashTableDestroy(CallArguments* aArguments) {
  auto& table = aArguments->Arg<0, PLHashTable*>();

  void* priv = table->allocPriv;
  CallFunction<void>(gOriginal_PL_HashTableDestroy, table);
  DestroyPLHashTableCallbacks(priv);
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// Objective C redirections
///////////////////////////////////////////////////////////////////////////////

// From Foundation.h, which has lots of Objective C declarations and can't be
// included here :(
struct NSFastEnumerationState {
  unsigned long state;
  id* itemsPtr;
  unsigned long* mutationsPtr;
  unsigned long extra[5];
};

// Emulation of NSFastEnumeration on arrays does not replay any exceptions
// thrown by mutating the array while it is being iterated over.
static unsigned long gNeverChange;

static PreambleResult Preamble_objc_msgSend(CallArguments* aArguments) {
  if (!AreThreadEventsPassedThrough()) {
    auto message = aArguments->Arg<1, const char*>();

    // Watch for some top level NSApplication messages that can cause Gecko
    // events to be processed.
    if (!strcmp(message, "run") ||
        !strcmp(message, "nextEventMatchingMask:untilDate:inMode:dequeue:")) {
      PassThroughThreadEventsAllowCallbacks([&]() {
        RecordReplayInvokeCall(gOriginal_objc_msgSend, aArguments);
      });
      RecordReplayBytes(&aArguments->Rval<size_t>(), sizeof(size_t));
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::Redirect;
}

static void RR_objc_msgSend(Stream& aEvents, CallArguments* aArguments,
                            ErrorType* aError) {
  auto& object = aArguments->Arg<0, id>();
  auto& message = aArguments->Arg<1, const char*>();

  aEvents.CheckInput(message);

  aEvents.RecordOrReplayValue(&aArguments->Rval<size_t>());
  aEvents.RecordOrReplayBytes(&aArguments->FloatRval(), sizeof(double));

  // Do some extra recording on messages that return additional state.

  if (!strcmp(message, "countByEnumeratingWithState:objects:count:")) {
    auto& state = aArguments->Arg<2, NSFastEnumerationState*>();
    auto& rval = aArguments->Rval<size_t>();
    if (IsReplaying()) {
      state->itemsPtr = NewLeakyArray<id>(rval);
      state->mutationsPtr = &gNeverChange;
    }
    aEvents.RecordOrReplayBytes(state->itemsPtr, rval * sizeof(id));
  }

  if (!strcmp(message, "getCharacters:")) {
    size_t len = 0;
    if (IsRecording()) {
      AutoPassThroughThreadEvents pt;
      len = CFStringGetLength((CFStringRef)object);
    }
    aEvents.RecordOrReplayValue(&len);
    aEvents.RecordOrReplayBytes(aArguments->Arg<2, void*>(),
                                len * sizeof(wchar_t));
  }

  if (!strcmp(message, "UTF8String") ||
      !strcmp(message, "cStringUsingEncoding:")) {
    auto& rval = aArguments->Rval<char*>();
    size_t len = IsRecording() ? strlen(rval) : 0;
    aEvents.RecordOrReplayValue(&len);
    if (IsReplaying()) {
      rval = NewLeakyArray<char>(len + 1);
    }
    aEvents.RecordOrReplayBytes(rval, len + 1);
  }
}

static void MM_Alloc(MiddlemanCallContext& aCx) {
  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    // Refuse to allocate NSAutoreleasePools in the middleman: the order in
    // which middleman calls happen does not guarantee the pools will be
    // created and released in LIFO order, as the pools require. Instead,
    // allocate an NSString instead so we have something to release later.
    // Messages sent to NSAutoreleasePools will all be skipped in the
    // middleman, so no one should notice this subterfuge.
    auto& obj = aCx.mArguments->Arg<0, id>();
    if (obj == (id)objc_lookUpClass("NSAutoreleasePool")) {
      obj = (id)objc_lookUpClass("NSString");
    }
  }

  MM_CreateCFTypeRval(aCx);
}

static void MM_PerformSelector(MiddlemanCallContext& aCx) {
  MM_CString<2>(aCx);
  MM_CFTypeArg<3>(aCx);

  // The behavior of performSelector:withObject: varies depending on the
  // selector used, so use a whitelist here.
  if (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) {
    auto str = aCx.mArguments->Arg<2, const char*>();
    if (strcmp(str, "appearanceNamed:")) {
      aCx.MarkAsFailed();
      return;
    }
  }

  MM_AutoreleaseCFTypeRval(aCx);
}

static void MM_DictionaryWithObjectsAndKeys(MiddlemanCallContext& aCx) {
  // Copy over all possible stack arguments.
  MM_StackArgumentData<CallArguments::NumStackArguments * sizeof(size_t)>(aCx);

  if (aCx.AccessPreface()) {
    // Advance through the arguments until there is a null value. If there are
    // too many arguments for the underlying CallArguments, we will safely
    // crash when we hit their extent.
    for (size_t i = 2;; i += 2) {
      auto& value = aCx.mArguments->Arg<id>(i);
      if (!value) {
        break;
      }
      auto& key = aCx.mArguments->Arg<id>(i + 1);
      MM_ObjCInput(aCx, &value);
      MM_ObjCInput(aCx, &key);
    }
  }

  MM_AutoreleaseCFTypeRval(aCx);
}

static void MM_DictionaryWithObjects(MiddlemanCallContext& aCx) {
  MM_Buffer<2, 4, const void*>(aCx);
  MM_Buffer<3, 4, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto objects = aCx.mArguments->Arg<2, const void**>();
    auto keys = aCx.mArguments->Arg<3, const void**>();
    auto count = aCx.mArguments->Arg<4, CFIndex>();

    for (CFIndex i = 0; i < count; i++) {
      MM_ObjCInput(aCx, (id*)&objects[i]);
      MM_ObjCInput(aCx, (id*)&keys[i]);
    }
  }

  MM_AutoreleaseCFTypeRval(aCx);
}

static void MM_NSStringGetCharacters(MiddlemanCallContext& aCx) {
  auto string = aCx.mArguments->Arg<0, CFStringRef>();
  auto& buffer = aCx.mArguments->Arg<2, void*>();

  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    size_t len = CFStringGetLength(string);
    buffer = aCx.AllocateBytes(len * sizeof(UniChar));
  }

  if (aCx.AccessOutput()) {
    size_t len = (aCx.mPhase == MiddlemanCallPhase::MiddlemanOutput)
                     ? CFStringGetLength(string)
                     : 0;
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mReplayOutputIsOld) {
      buffer = aCx.AllocateBytes(len * sizeof(UniChar));
    }
    aCx.ReadOrWriteOutputBytes(buffer, len * sizeof(UniChar));
  }
}

struct ObjCMessageInfo {
  const char* mMessage;
  MiddlemanCallFn mMiddlemanCall;
  bool mUpdatesObject;
};

// All Objective C messages that can be called in the middleman, and hooks for
// capturing any inputs and outputs other than the object, message, and scalar
// arguments / return values.
static ObjCMessageInfo gObjCMiddlemanCallMessages[] = {
    // Generic
    {"alloc", MM_Alloc},
    {"init", MM_AutoreleaseCFTypeRval},
    {"performSelector:withObject:", MM_PerformSelector},
    {"release", MM_SkipInMiddleman},
    {"respondsToSelector:", MM_CString<2>},

    // NSAppearance
    {"_drawInRect:context:options:",
     MM_Compose<MM_StackArgumentData<sizeof(CGRect)>, MM_CFTypeArg<2>,
                MM_CFTypeArg<3>>},

    // NSAutoreleasePool
    {"drain", MM_SkipInMiddleman},

    // NSArray
    {"count"},
    {"objectAtIndex:", MM_AutoreleaseCFTypeRval},

    // NSBezierPath
    {"addClip", MM_NoOp, true},
    {"bezierPathWithRoundedRect:xRadius:yRadius:", MM_AutoreleaseCFTypeRval},

    // NSCell
    {"drawFocusRingMaskWithFrame:inView:",
     MM_Compose<MM_CFTypeArg<2>, MM_StackArgumentData<sizeof(CGRect)>>},
    {"drawWithFrame:inView:",
     MM_Compose<MM_CFTypeArg<2>, MM_StackArgumentData<sizeof(CGRect)>>},
    {"initTextCell:", MM_Compose<MM_CFTypeArg<2>, MM_AutoreleaseCFTypeRval>},
    {"initTextCell:pullsDown:",
     MM_Compose<MM_CFTypeArg<2>, MM_AutoreleaseCFTypeRval>},
    {"setAllowsMixedState:", MM_NoOp, true},
    {"setBezeled:", MM_NoOp, true},
    {"setBezelStyle:", MM_NoOp, true},
    {"setButtonType:", MM_NoOp, true},
    {"setControlSize:", MM_NoOp, true},
    {"setControlTint:", MM_NoOp, true},
    {"setCriticalValue:", MM_NoOp, true},
    {"setDoubleValue:", MM_NoOp, true},
    {"setEditable:", MM_NoOp, true},
    {"setEnabled:", MM_NoOp, true},
    {"setFocusRingType:", MM_NoOp, true},
    {"setHighlighted:", MM_NoOp, true},
    {"setHighlightsBy:", MM_NoOp, true},
    {"setHorizontal:", MM_NoOp, true},
    {"setIndeterminate:", MM_NoOp, true},
    {"setMax:", MM_NoOp, true},
    {"setMaxValue:", MM_NoOp, true},
    {"setMinValue:", MM_NoOp, true},
    {"setPlaceholderString:", MM_NoOp, true},
    {"setPullsDown:", MM_NoOp, true},
    {"setShowsFirstResponder:", MM_NoOp, true},
    {"setState:", MM_NoOp, true},
    {"setValue:", MM_NoOp, true},
    {"setWarningValue:", MM_NoOp, true},
    {"showsFirstResponder"},

    // NSColor
    {"alphaComponent"},
    {"colorWithDeviceRed:green:blue:alpha:",
     MM_Compose<MM_StackArgumentData<sizeof(CGFloat)>,
                MM_AutoreleaseCFTypeRval>},
    {"currentControlTint"},
    {"set", MM_NoOp, true},

    // NSDictionary
    {"dictionaryWithObjectsAndKeys:", MM_DictionaryWithObjectsAndKeys},
    {"dictionaryWithObjects:forKeys:count:", MM_DictionaryWithObjects},
    {"mutableCopy", MM_AutoreleaseCFTypeRval},
    {"setObject:forKey:", MM_Compose<MM_CFTypeArg<2>, MM_CFTypeArg<3>>, true},

    // NSFont
    {"boldSystemFontOfSize:", MM_AutoreleaseCFTypeRval},
    {"controlContentFontOfSize:", MM_AutoreleaseCFTypeRval},
    {"familyName", MM_AutoreleaseCFTypeRval},
    {"fontDescriptor", MM_AutoreleaseCFTypeRval},
    {"menuBarFontOfSize:", MM_AutoreleaseCFTypeRval},
    {"pointSize"},
    {"smallSystemFontSize"},
    {"systemFontOfSize:", MM_AutoreleaseCFTypeRval},
    {"toolTipsFontOfSize:", MM_AutoreleaseCFTypeRval},
    {"userFontOfSize:", MM_AutoreleaseCFTypeRval},

    // NSFontManager
    {"availableMembersOfFontFamily:",
     MM_Compose<MM_CFTypeArg<2>, MM_AutoreleaseCFTypeRval>},
    {"sharedFontManager", MM_AutoreleaseCFTypeRval},

    // NSGraphicsContext
    {"currentContext", MM_AutoreleaseCFTypeRval},
    {"graphicsContextWithGraphicsPort:flipped:",
     MM_Compose<MM_CFTypeArg<2>, MM_AutoreleaseCFTypeRval>},
    {"graphicsPort", MM_AutoreleaseCFTypeRval},
    {"restoreGraphicsState"},
    {"saveGraphicsState"},
    {"setCurrentContext:", MM_CFTypeArg<2>},

    // NSNumber
    {"numberWithBool:", MM_AutoreleaseCFTypeRval},
    {"unsignedIntValue"},

    // NSString
    {"getCharacters:", MM_NSStringGetCharacters},
    {"hasSuffix:", MM_CFTypeArg<2>},
    {"isEqualToString:", MM_CFTypeArg<2>},
    {"length"},
    {"rangeOfString:options:", MM_CFTypeArg<2>},
    {"stringWithCharacters:length:",
     MM_Compose<MM_Buffer<2, 3, UniChar>, MM_AutoreleaseCFTypeRval>},

    // NSWindow
    {"coreUIRenderer", MM_AutoreleaseCFTypeRval},

    // UIFontDescriptor
    {"symbolicTraits"},
};

static void MM_objc_msgSend(MiddlemanCallContext& aCx) {
  auto message = aCx.mArguments->Arg<1, const char*>();

  for (const ObjCMessageInfo& info : gObjCMiddlemanCallMessages) {
    if (!strcmp(info.mMessage, message)) {
      if (info.mUpdatesObject) {
        MM_UpdateCFTypeArg<0>(aCx);
      } else {
        MM_CFTypeArg<0>(aCx);
      }
      if (info.mMiddlemanCall && !aCx.mFailed) {
        info.mMiddlemanCall(aCx);
      }
      if (child::CurrentRepaintCannotFail() && aCx.mFailed) {
        child::ReportFatalError(Nothing(), "Middleman message failure: %s\n",
                                message);
      }
      return;
    }
  }

  if (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) {
    aCx.MarkAsFailed();
    if (child::CurrentRepaintCannotFail()) {
      child::ReportFatalError(
          Nothing(), "Could not perform middleman message: %s\n", message);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Cocoa redirections
///////////////////////////////////////////////////////////////////////////////

static void MM_CFArrayCreate(MiddlemanCallContext& aCx) {
  MM_Buffer<1, 2, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto values = aCx.mArguments->Arg<1, const void**>();
    auto numValues = aCx.mArguments->Arg<2, CFIndex>();
    auto& callbacks = aCx.mArguments->Arg<3, const CFArrayCallBacks*>();

    // For now we only support creating arrays with CFType elements.
    if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
      callbacks = &kCFTypeArrayCallBacks;
    } else {
      MOZ_RELEASE_ASSERT(callbacks == &kCFTypeArrayCallBacks);
    }

    for (CFIndex i = 0; i < numValues; i++) {
      MM_ObjCInput(aCx, (id*)&values[i]);
    }
  }

  MM_CreateCFTypeRval(aCx);
}

static void MM_CFArrayGetValueAtIndex(MiddlemanCallContext& aCx) {
  MM_CFTypeArg<0>(aCx);

  auto array = aCx.mArguments->Arg<0, id>();

  // We can't probe the array to see what callbacks it uses, so look at where
  // it came from to see whether its elements should be treated as CFTypes.
  MiddlemanCall* call = LookupMiddlemanCall(array);
  bool isCFTypeRval = false;
  if (call) {
    const char* name = GetRedirection(call->mCallId).mName;
    if (!strcmp(name, "CTLineGetGlyphRuns") ||
        !strcmp(name, "CTFontCopyVariationAxes") ||
        !strcmp(name, "CTFontDescriptorCreateMatchingFontDescriptors")) {
      isCFTypeRval = true;
    }
  }

  if (isCFTypeRval) {
    MM_CFTypeRval(aCx);
  }
}

static void RR_CFDataGetBytePtr(Stream& aEvents, CallArguments* aArguments,
                                ErrorType* aError) {
  auto& rval = aArguments->Rval<UInt8*>();

  size_t len = 0;
  if (IsRecording()) {
    len = CallFunction<size_t>(gOriginal_CFDataGetLength,
                               aArguments->Arg<0, CFDataRef>());
  }
  aEvents.RecordOrReplayValue(&len);
  if (IsReplaying()) {
    rval = NewLeakyArray<UInt8>(len);
  }
  aEvents.RecordOrReplayBytes(rval, len);
}

static void MM_CFDataGetBytePtr(MiddlemanCallContext& aCx) {
  MM_CFTypeArg<0>(aCx);

  auto data = aCx.mArguments->Arg<0, CFDataRef>();
  auto& buffer = aCx.mArguments->Rval<void*>();

  if (aCx.AccessOutput()) {
    size_t len = (aCx.mPhase == MiddlemanCallPhase::MiddlemanOutput)
                     ? CFDataGetLength(data)
                     : 0;
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mPhase == MiddlemanCallPhase::ReplayOutput) {
      buffer = aCx.AllocateBytes(len);
    }
    aCx.ReadOrWriteOutputBytes(buffer, len);
  }
}

static void MM_CFDictionaryCreate(MiddlemanCallContext& aCx) {
  MM_Buffer<1, 3, const void*>(aCx);
  MM_Buffer<2, 3, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto keys = aCx.mArguments->Arg<1, const void**>();
    auto values = aCx.mArguments->Arg<2, const void**>();
    auto numValues = aCx.mArguments->Arg<3, CFIndex>();
    auto& keyCallbacks =
        aCx.mArguments->Arg<4, const CFDictionaryKeyCallBacks*>();
    auto& valueCallbacks =
        aCx.mArguments->Arg<5, const CFDictionaryValueCallBacks*>();

    // For now we only support creating dictionaries with CFType keys and
    // values.
    if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
      keyCallbacks = &kCFTypeDictionaryKeyCallBacks;
      valueCallbacks = &kCFTypeDictionaryValueCallBacks;
    } else {
      MOZ_RELEASE_ASSERT(keyCallbacks == &kCFTypeDictionaryKeyCallBacks);
      MOZ_RELEASE_ASSERT(valueCallbacks == &kCFTypeDictionaryValueCallBacks);
    }

    for (CFIndex i = 0; i < numValues; i++) {
      MM_ObjCInput(aCx, (id*)&keys[i]);
      MM_ObjCInput(aCx, (id*)&values[i]);
    }
  }

  MM_CreateCFTypeRval(aCx);
}

static void DummyCFNotificationCallback(CFNotificationCenterRef aCenter,
                                        void* aObserver, CFStringRef aName,
                                        const void* aObject,
                                        CFDictionaryRef aUserInfo) {
  // FIXME
  // MOZ_CRASH();
}

static PreambleResult Preamble_CFNotificationCenterAddObserver(
    CallArguments* aArguments) {
  auto& callback = aArguments->Arg<2, CFNotificationCallback>();
  if (!AreThreadEventsPassedThrough()) {
    callback = DummyCFNotificationCallback;
  }
  return PreambleResult::Redirect;
}

static size_t CFNumberTypeBytes(CFNumberType aType) {
  switch (aType) {
    case kCFNumberSInt8Type:
      return sizeof(int8_t);
    case kCFNumberSInt16Type:
      return sizeof(int16_t);
    case kCFNumberSInt32Type:
      return sizeof(int32_t);
    case kCFNumberSInt64Type:
      return sizeof(int64_t);
    case kCFNumberFloat32Type:
      return sizeof(float);
    case kCFNumberFloat64Type:
      return sizeof(double);
    case kCFNumberCharType:
      return sizeof(char);
    case kCFNumberShortType:
      return sizeof(short);
    case kCFNumberIntType:
      return sizeof(int);
    case kCFNumberLongType:
      return sizeof(long);
    case kCFNumberLongLongType:
      return sizeof(long long);
    case kCFNumberFloatType:
      return sizeof(float);
    case kCFNumberDoubleType:
      return sizeof(double);
    case kCFNumberCFIndexType:
      return sizeof(CFIndex);
    case kCFNumberNSIntegerType:
      return sizeof(long);
    case kCFNumberCGFloatType:
      return sizeof(CGFloat);
    default:
      MOZ_CRASH();
  }
}

static void MM_CFNumberCreate(MiddlemanCallContext& aCx) {
  if (aCx.AccessPreface()) {
    auto numberType = aCx.mArguments->Arg<1, CFNumberType>();
    auto& valuePtr = aCx.mArguments->Arg<2, void*>();
    aCx.ReadOrWritePrefaceBuffer(&valuePtr, CFNumberTypeBytes(numberType));
  }

  MM_CreateCFTypeRval(aCx);
}

static void RR_CFNumberGetValue(Stream& aEvents, CallArguments* aArguments,
                                ErrorType* aError) {
  auto& type = aArguments->Arg<1, CFNumberType>();
  auto& value = aArguments->Arg<2, void*>();

  aEvents.CheckInput(type);
  aEvents.RecordOrReplayBytes(value, CFNumberTypeBytes(type));
}

static void MM_CFNumberGetValue(MiddlemanCallContext& aCx) {
  MM_CFTypeArg<0>(aCx);

  auto& buffer = aCx.mArguments->Arg<2, void*>();
  auto type = aCx.mArguments->Arg<1, CFNumberType>();
  aCx.ReadOrWriteOutputBuffer(&buffer, CFNumberTypeBytes(type));
}

static PreambleResult MiddlemanPreamble_CFRetain(CallArguments* aArguments) {
  aArguments->Rval<size_t>() = aArguments->Arg<0, size_t>();
  return PreambleResult::Veto;
}

static PreambleResult Preamble_CFRunLoopSourceCreate(
    CallArguments* aArguments) {
  if (!AreThreadEventsPassedThrough()) {
    auto& cx = aArguments->Arg<2, CFRunLoopSourceContext*>();

    RegisterCallbackData(BitwiseCast<void*>(cx->perform));
    RegisterCallbackData(cx->info);

    if (IsRecording()) {
      CallbackWrapperData* wrapperData =
          new CallbackWrapperData(cx->perform, cx->info);
      cx->perform = CFRunLoopPerformCallBackWrapper;
      cx->info = wrapperData;
    }
  }
  return PreambleResult::Redirect;
}

struct ContextDataInfo {
  CGContextRef mContext;
  void* mData;
  size_t mDataSize;

  ContextDataInfo(CGContextRef aContext, void* aData, size_t aDataSize)
      : mContext(aContext), mData(aData), mDataSize(aDataSize) {}
};

static StaticInfallibleVector<ContextDataInfo> gContextData;

static void RR_CGBitmapContextCreateWithData(Stream& aEvents,
                                             CallArguments* aArguments,
                                             ErrorType* aError) {
  auto& data = aArguments->Arg<0, void*>();
  auto& height = aArguments->Arg<2, size_t>();
  auto& bytesPerRow = aArguments->Arg<4, size_t>();
  auto& rval = aArguments->Rval<CGContextRef>();

  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // When replaying, MM_CGBitmapContextCreateWithData will take care of
  // updating gContextData with the right context pointer (after being mangled
  // in MM_SystemOutput).
  if (IsRecording()) {
    gContextData.emplaceBack(rval, data, height * bytesPerRow);
  }
}

static void MM_CGBitmapContextCreateWithData(MiddlemanCallContext& aCx) {
  MM_CFTypeArg<5>(aCx);
  MM_StackArgumentData<3 * sizeof(size_t)>(aCx);
  MM_CreateCFTypeRval(aCx);

  auto& data = aCx.mArguments->Arg<0, void*>();
  auto height = aCx.mArguments->Arg<2, size_t>();
  auto bytesPerRow = aCx.mArguments->Arg<4, size_t>();
  auto rval = aCx.mArguments->Rval<CGContextRef>();

  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    data = aCx.AllocateBytes(height * bytesPerRow);
  }

  if ((aCx.mPhase == MiddlemanCallPhase::ReplayPreface &&
       !HasDivergedFromRecording()) ||
      (aCx.AccessOutput() && !aCx.mReplayOutputIsOld)) {
    gContextData.emplaceBack(rval, data, height * bytesPerRow);
  }
}

template <size_t ContextArgument>
static void RR_FlushCGContext(Stream& aEvents, CallArguments* aArguments,
                              ErrorType* aError) {
  auto& context = aArguments->Arg<ContextArgument, CGContextRef>();

  for (int i = gContextData.length() - 1; i >= 0; i--) {
    if (context == gContextData[i].mContext) {
      aEvents.RecordOrReplayBytes(gContextData[i].mData,
                                  gContextData[i].mDataSize);
      return;
    }
  }
  MOZ_CRASH("RR_FlushCGContext");
}

template <size_t ContextArgument>
static void MM_FlushCGContext(MiddlemanCallContext& aCx) {
  auto context = aCx.mArguments->Arg<ContextArgument, CGContextRef>();

  // Update the contents of the target buffer in the middleman process to match
  // the current contents in the replaying process.
  if (aCx.AccessInput()) {
    for (int i = gContextData.length() - 1; i >= 0; i--) {
      if (context == gContextData[i].mContext) {
        aCx.ReadOrWriteInputBytes(gContextData[i].mData,
                                  gContextData[i].mDataSize);
        return;
      }
    }
    MOZ_CRASH("MM_FlushCGContext");
  }

  // After performing the call, the buffer in the replaying process is updated
  // to match any data written by the middleman.
  if (aCx.AccessOutput()) {
    for (int i = gContextData.length() - 1; i >= 0; i--) {
      if (context == gContextData[i].mContext) {
        aCx.ReadOrWriteOutputBytes(gContextData[i].mData,
                                   gContextData[i].mDataSize);
        return;
      }
    }
    MOZ_CRASH("MM_FlushCGContext");
  }
}

static PreambleResult Preamble_CGContextRestoreGState(
    CallArguments* aArguments) {
  return IsRecording() ? PreambleResult::PassThrough : PreambleResult::Veto;
}

static void RR_CGDataProviderCreateWithData(Stream& aEvents,
                                            CallArguments* aArguments,
                                            ErrorType* aError) {
  auto& info = aArguments->Arg<0, void*>();
  auto& data = aArguments->Arg<1, const void*>();
  auto& size = aArguments->Arg<2, size_t>();
  auto& releaseData = aArguments->Arg<3, CGDataProviderReleaseDataCallback>();

  if (IsReplaying() && releaseData) {
    // Immediately release the data, since there is no data provider to do it
    // for us.
    releaseData(info, data, size);
  }
}

static void ReleaseDataCallback(void*, const void* aData, size_t) {
  free((void*)aData);
}

static void MM_CGDataProviderCreateWithData(MiddlemanCallContext& aCx) {
  MM_Buffer<1, 2>(aCx);
  MM_CreateCFTypeRval(aCx);

  auto& info = aCx.mArguments->Arg<0, void*>();
  auto& data = aCx.mArguments->Arg<1, const void*>();
  auto& size = aCx.mArguments->Arg<2, size_t>();
  auto& releaseData =
      aCx.mArguments->Arg<3, CGDataProviderReleaseDataCallback>();

  // Make a copy of the data that won't be released the next time middleman
  // calls are reset, in case CoreGraphics decides to hang onto the data
  // provider after that point.
  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    void* newData = malloc(size);
    memcpy(newData, data, size);
    data = newData;
    releaseData = ReleaseDataCallback;
  }

  // Immediately release the data in the replaying process.
  if (aCx.mPhase == MiddlemanCallPhase::ReplayInput && releaseData) {
    releaseData(info, data, size);
  }
}

static PreambleResult Preamble_CGPathApply(CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& path = aArguments->Arg<0, CGPathRef>();
  auto& data = aArguments->Arg<1, void*>();
  auto& function = aArguments->Arg<2, CGPathApplierFunction>();

  RegisterCallbackData(BitwiseCast<void*>(function));
  RegisterCallbackData(data);
  PassThroughThreadEventsAllowCallbacks([&]() {
    CallbackWrapperData wrapperData(function, data);
    CallFunction<void>(gOriginal_CGPathApply, path, &wrapperData,
                       CGPathApplierFunctionWrapper);
  });
  RemoveCallbackData(data);

  return PreambleResult::Veto;
}

// Note: We only redirect CTRunGetGlyphsPtr, not CTRunGetGlyphs. The latter may
// be implemented with a loop that jumps back into the code we overwrite with a
// jump, a pattern which ProcessRedirect.cpp does not handle. For the moment,
// Gecko code only calls CTRunGetGlyphs if CTRunGetGlyphsPtr fails, so make
// sure that CTRunGetGlyphsPtr always succeeds when it is being recorded.
// The same concerns apply to CTRunGetPositionsPtr and CTRunGetStringIndicesPtr,
// so they are all handled here.
template <typename ElemType, void (*GetElemsFn)(CTRunRef, CFRange, ElemType*)>
static void RR_CTRunGetElements(Stream& aEvents, CallArguments* aArguments,
                                ErrorType* aError) {
  auto& run = aArguments->Arg<0, CTRunRef>();
  auto& rval = aArguments->Rval<ElemType*>();

  size_t count;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    count = CTRunGetGlyphCount(run);
    if (!rval) {
      rval = NewLeakyArray<ElemType>(count);
      GetElemsFn(run, CFRangeMake(0, 0), rval);
    }
  }
  aEvents.RecordOrReplayValue(&count);
  if (IsReplaying()) {
    rval = NewLeakyArray<ElemType>(count);
  }
  aEvents.RecordOrReplayBytes(rval, count * sizeof(ElemType));
}

template <typename ElemType, void (*GetElemsFn)(CTRunRef, CFRange, ElemType*)>
static void MM_CTRunGetElements(MiddlemanCallContext& aCx) {
  MM_CFTypeArg<0>(aCx);

  if (aCx.AccessOutput()) {
    auto run = aCx.mArguments->Arg<0, CTRunRef>();
    auto& rval = aCx.mArguments->Rval<ElemType*>();

    size_t count = 0;
    if (IsMiddleman()) {
      count = CTRunGetGlyphCount(run);
      if (!rval) {
        rval = (ElemType*)aCx.AllocateBytes(count * sizeof(ElemType));
        GetElemsFn(run, CFRangeMake(0, 0), rval);
      }
    }
    aCx.ReadOrWriteOutputBytes(&count, sizeof(count));
    if (IsReplaying()) {
      rval = (ElemType*)aCx.AllocateBytes(count * sizeof(ElemType));
    }
    aCx.ReadOrWriteOutputBytes(rval, count * sizeof(ElemType));
  }
}

static PreambleResult Preamble_OSSpinLockLock(CallArguments* aArguments) {
  auto& lock = aArguments->Arg<0, OSSpinLock*>();

  // These spin locks never need to be recorded, but they are used by malloc
  // and can end up calling other recorded functions like mach_absolute_time,
  // so make sure events are passed through here. Note that we don't have to
  // redirect OSSpinLockUnlock, as it doesn't have these issues.
  AutoEnsurePassThroughThreadEventsUseStackPointer pt;
  CallFunction<void>(gOriginal_OSSpinLockLock, lock);

  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// System Redirections
///////////////////////////////////////////////////////////////////////////////

// System APIs that are redirected, and associated callbacks.
struct SystemRedirection {
  const char* mName;
  SaveOutputFn mSaveOutput;
  PreambleFn mPreamble;
  MiddlemanCallFn mMiddlemanCall;
  PreambleFn mMiddlemanPreamble;
};

// Specify every library function that is redirected by looking up its address
// with dlsym.
static SystemRedirection gSystemRedirections[] = {
    /////////////////////////////////////////////////////////////////////////////
    // System call wrappers
    /////////////////////////////////////////////////////////////////////////////

    {"kevent", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<3, 4, struct kevent>>,
     nullptr, nullptr, Preamble_WaitForever},
    {"kevent64",
     RR_SaveRvalHadErrorNegative<RR_WriteBuffer<3, 4, struct kevent64_s>>},
    {"mprotect", nullptr, Preamble_mprotect},
    {"mmap", nullptr, Preamble_mmap},
    {"munmap", nullptr, Preamble_munmap},
    {"read", RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>, nullptr,
     nullptr, Preamble_SetError<EIO>},
    {"__read_nocancel",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>},
    {"pread", RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>},
    {"write", RR_SaveRvalHadErrorNegative, nullptr, nullptr,
     MiddlemanPreamble_write},
    {"__write_nocancel", RR_SaveRvalHadErrorNegative},
    {"open", RR_SaveRvalHadErrorNegative},
    {"__open_nocancel", RR_SaveRvalHadErrorNegative},
    {"recv", RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>},
    {"recvmsg", RR_SaveRvalHadErrorNegative<RR_recvmsg>, nullptr, nullptr,
     Preamble_WaitForever},
    {"sendmsg", RR_SaveRvalHadErrorNegative, nullptr, nullptr,
     MiddlemanPreamble_sendmsg},
    {"shm_open", RR_SaveRvalHadErrorNegative},
    {"socket", RR_SaveRvalHadErrorNegative},
    {"kqueue", RR_SaveRvalHadErrorNegative},
    {"pipe",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<0, 2 * sizeof(int)>>,
     nullptr, nullptr, Preamble_SetError},
    {"close", RR_SaveRvalHadErrorNegative, nullptr, nullptr, Preamble_Veto<0>},
    {"__close_nocancel", RR_SaveRvalHadErrorNegative},
    {"mkdir", RR_SaveRvalHadErrorNegative},
    {"dup", RR_SaveRvalHadErrorNegative},
    {"access", RR_SaveRvalHadErrorNegative, nullptr, nullptr,
     Preamble_SetError<EACCES>},
    {"lseek", RR_SaveRvalHadErrorNegative},
    {"socketpair",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<3, 2 * sizeof(int)>>},
    {"fileport_makeport",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(size_t)>>},
    {"getsockopt", RR_SaveRvalHadErrorNegative<RR_getsockopt>},
    {"gettimeofday",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_WriteOptionalBufferFixedSize<0, sizeof(struct timeval)>,
         RR_WriteOptionalBufferFixedSize<1, sizeof(struct timezone)>>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"getuid", RR_ScalarRval},
    {"geteuid", RR_ScalarRval},
    {"getgid", RR_ScalarRval},
    {"getegid", RR_ScalarRval},
    {"issetugid", RR_ScalarRval},
    {"__gettid", RR_ScalarRval},
    {"getpid", nullptr, Preamble_getpid},
    {"fcntl", RR_SaveRvalHadErrorNegative, Preamble_fcntl, nullptr,
     MiddlemanPreamble_fcntl},
    {"getattrlist", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 3>>},
    {"fstat$INODE64",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct stat)>>,
     nullptr, nullptr, Preamble_SetError},
    {"lstat$INODE64",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct stat)>>,
     nullptr, nullptr, Preamble_SetError},
    {"stat$INODE64",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct stat)>>,
     nullptr, nullptr, Preamble_SetError},
    {"statfs$INODE64",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct statfs)>>,
     nullptr, nullptr, Preamble_SetError},
    {"fstatfs$INODE64",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct statfs)>>,
     nullptr, nullptr, Preamble_SetError},
    {"readlink", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<1, 2>>},
    {"__getdirentries64",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_WriteBuffer<1, 2>, RR_WriteBufferFixedSize<3, sizeof(size_t)>>>},
    {"getdirentriesattr",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_WriteBufferFixedSize<1, sizeof(struct attrlist)>,
         RR_WriteBuffer<2, 3>, RR_WriteBufferFixedSize<4, sizeof(size_t)>,
         RR_WriteBufferFixedSize<5, sizeof(size_t)>,
         RR_WriteBufferFixedSize<6, sizeof(size_t)>>>},
    {"getrusage",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<1, sizeof(struct rusage)>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"__getrlimit", RR_SaveRvalHadErrorNegative<
                        RR_WriteBufferFixedSize<1, sizeof(struct rlimit)>>},
    {"__setrlimit", RR_SaveRvalHadErrorNegative},
    {"sigprocmask",
     RR_SaveRvalHadErrorNegative<
         RR_WriteOptionalBufferFixedSize<2, sizeof(sigset_t)>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"sigaltstack", RR_SaveRvalHadErrorNegative<
                        RR_WriteOptionalBufferFixedSize<2, sizeof(stack_t)>>},
    {"sigaction",
     RR_SaveRvalHadErrorNegative<
         RR_WriteOptionalBufferFixedSize<2, sizeof(struct sigaction)>>},
    {"__pthread_sigmask",
     RR_SaveRvalHadErrorNegative<
         RR_WriteOptionalBufferFixedSize<2, sizeof(sigset_t)>>},
    {"__fsgetpath", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>},
    {"__disable_threadsignal", nullptr, Preamble___disable_threadsignal},
    {"__sysctl", RR_SaveRvalHadErrorNegative<RR___sysctl>},
    {"__mac_syscall", RR_SaveRvalHadErrorNegative},
    {"getaudit_addr",
     RR_SaveRvalHadErrorNegative<
         RR_WriteBufferFixedSize<0, sizeof(auditinfo_addr_t)>>},
    {"umask", RR_ScalarRval},
    {"__select",
     RR_SaveRvalHadErrorNegative<
         RR_Compose<RR_WriteBufferFixedSize<1, sizeof(fd_set)>,
                    RR_WriteBufferFixedSize<2, sizeof(fd_set)>,
                    RR_WriteBufferFixedSize<3, sizeof(fd_set)>,
                    RR_WriteOptionalBufferFixedSize<4, sizeof(timeval)>>>,
     nullptr, nullptr, Preamble_WaitForever},
    {"__process_policy", RR_SaveRvalHadErrorNegative},
    {"__kdebug_trace", RR_SaveRvalHadErrorNegative},
    {"guarded_kqueue_np",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<0, sizeof(size_t)>>},
    {"csops", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 3>>},
    {"__getlogin", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>},
    {"__workq_kernreturn", nullptr, Preamble___workq_kernreturn},
    {"start_wqthread", nullptr, Preamble_start_wqthread},

    /////////////////////////////////////////////////////////////////////////////
    // PThreads interfaces
    /////////////////////////////////////////////////////////////////////////////

    {"pthread_cond_wait", nullptr, Preamble_pthread_cond_wait},
    {"pthread_cond_timedwait", nullptr, Preamble_pthread_cond_timedwait},
    {"pthread_cond_timedwait_relative_np", nullptr,
     Preamble_pthread_cond_timedwait_relative_np},
    {"pthread_create", nullptr, Preamble_pthread_create},
    {"pthread_join", nullptr, Preamble_pthread_join},
    {"pthread_mutex_init", nullptr, Preamble_pthread_mutex_init},
    {"pthread_mutex_destroy", nullptr, Preamble_pthread_mutex_destroy},
    {"pthread_mutex_lock", nullptr, Preamble_pthread_mutex_lock},
    {"pthread_mutex_trylock", nullptr, Preamble_pthread_mutex_trylock},
    {"pthread_mutex_unlock", nullptr, Preamble_pthread_mutex_unlock},

    /////////////////////////////////////////////////////////////////////////////
    // C library functions
    /////////////////////////////////////////////////////////////////////////////

    {"dlclose", nullptr, Preamble_Veto<0>},
    {"dlopen", nullptr, Preamble_PassThrough},
    {"dlsym", nullptr, Preamble_PassThrough},
    {"fclose", RR_SaveRvalHadErrorNegative},
    {"fopen", RR_SaveRvalHadErrorZero},
    {"fread", RR_Compose<RR_ScalarRval, RR_fread>},
    {"fseek", RR_SaveRvalHadErrorNegative},
    {"ftell", RR_SaveRvalHadErrorNegative},
    {"fwrite", RR_ScalarRval},
    {"getenv", RR_CStringRval, Preamble_getenv, nullptr, Preamble_Veto<0>},
    {"localtime_r",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_WriteBufferFixedSize<1, sizeof(struct tm)>, RR_RvalIsArgument<1>>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"gmtime_r",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_WriteBufferFixedSize<1, sizeof(struct tm)>, RR_RvalIsArgument<1>>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"localtime", nullptr, Preamble_localtime, nullptr, Preamble_PassThrough},
    {"gmtime", nullptr, Preamble_gmtime, nullptr, Preamble_PassThrough},
    {"mktime",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<0, sizeof(struct tm)>>},
    {"setlocale", RR_CStringRval},
    {"strftime", RR_Compose<RR_ScalarRval, RR_WriteBufferViaRval<0, 1, 1>>},
    {"arc4random", RR_ScalarRval, nullptr, nullptr, Preamble_PassThrough},
    {"mach_absolute_time", RR_ScalarRval, Preamble_mach_absolute_time, nullptr,
     Preamble_PassThrough},
    {"mach_msg", RR_Compose<RR_ScalarRval, RR_WriteBuffer<0, 3>>, nullptr,
     nullptr, Preamble_WaitForever},
    {"mach_timebase_info",
     RR_Compose<RR_ScalarRval,
                RR_WriteBufferFixedSize<0, sizeof(mach_timebase_info_data_t)>>},
    {"mach_vm_allocate", nullptr, Preamble_mach_vm_allocate},
    {"mach_vm_deallocate", nullptr, Preamble_mach_vm_deallocate},
    {"mach_vm_map", nullptr, Preamble_mach_vm_map},
    {"mach_vm_protect", nullptr, Preamble_mach_vm_protect},
    {"rand", RR_ScalarRval},
    {"realpath",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_CStringRval, RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>},
    {"realpath$DARWIN_EXTSN",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_CStringRval, RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>},

    // By passing through events when initializing the sandbox, we ensure both
    // that we actually initialize the process sandbox while replaying as well
    // as while recording, and that any activity in these calls does not
    // interfere with the replay.
    {"sandbox_init", nullptr, Preamble_PassThrough},
    {"sandbox_init_with_parameters", nullptr, Preamble_PassThrough},

    // Make sure events are passed through here so that replaying processes can
    // inspect their own threads.
    {"task_threads", nullptr, Preamble_PassThrough},

    {"vm_copy", nullptr, Preamble_vm_copy},
    {"vm_purgable_control", nullptr, Preamble_vm_purgable_control},
    {"tzset"},

    /////////////////////////////////////////////////////////////////////////////
    // Gecko functions
    /////////////////////////////////////////////////////////////////////////////

    // These are defined in NSPR, but it is easier to just redirect them to
    // change their behavior than to actually modify their code.
    {"PL_NewHashTable", nullptr, Preamble_PL_NewHashTable},
    {"PL_HashTableDestroy", nullptr, Preamble_PL_HashTableDestroy},

    /////////////////////////////////////////////////////////////////////////////
    // Objective C functions
    /////////////////////////////////////////////////////////////////////////////

    {"class_getClassMethod", RR_ScalarRval},
    {"class_getInstanceMethod", RR_ScalarRval},
    {"method_exchangeImplementations"},
    {"objc_autoreleasePoolPop"},
    {"objc_autoreleasePoolPush", RR_ScalarRval},
    {"objc_msgSend", RR_objc_msgSend, Preamble_objc_msgSend, MM_objc_msgSend},

    /////////////////////////////////////////////////////////////////////////////
    // Cocoa and CoreFoundation library functions
    /////////////////////////////////////////////////////////////////////////////

    {"AcquireFirstMatchingEventInQueue", RR_ScalarRval},
    {"CFArrayAppendValue"},
    {"CFArrayCreate", RR_ScalarRval, nullptr, MM_CFArrayCreate},
    {"CFArrayGetCount", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CFArrayGetValueAtIndex", RR_ScalarRval, nullptr,
     MM_CFArrayGetValueAtIndex},
    {"CFArrayRemoveValueAtIndex"},
    {"CFAttributedStringCreate", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<1>, MM_CFTypeArg<2>, MM_CreateCFTypeRval>},
    {"CFBundleCopyExecutableURL", RR_ScalarRval},
    {"CFBundleCopyInfoDictionaryForURL", RR_ScalarRval},
    {"CFBundleCreate", RR_ScalarRval},
    {"CFBundleGetBundleWithIdentifier", RR_ScalarRval},
    {"CFBundleGetDataPointerForName", nullptr,
     Preamble_VetoIfNotPassedThrough<0>},
    {"CFBundleGetFunctionPointerForName", nullptr,
     Preamble_VetoIfNotPassedThrough<0>},
    {"CFBundleGetIdentifier", RR_ScalarRval},
    {"CFBundleGetInfoDictionary", RR_ScalarRval},
    {"CFBundleGetMainBundle", RR_ScalarRval},
    {"CFBundleGetValueForInfoDictionaryKey", RR_ScalarRval},
    {"CFDataGetBytePtr", RR_CFDataGetBytePtr, nullptr, MM_CFDataGetBytePtr},
    {"CFDataGetLength", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CFDateFormatterCreate", RR_ScalarRval},
    {"CFDateFormatterGetFormat", RR_ScalarRval},
    {"CFDictionaryAddValue", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>, MM_CFTypeArg<1>, MM_CFTypeArg<2>>},
    {"CFDictionaryCreate", RR_ScalarRval, nullptr, MM_CFDictionaryCreate},
    {"CFDictionaryCreateMutable", RR_ScalarRval, nullptr, MM_CreateCFTypeRval},
    {"CFDictionaryCreateMutableCopy", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<2>, MM_CreateCFTypeRval>},
    {"CFDictionaryGetValue", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CFTypeRval>},
    {"CFDictionaryGetValueIfPresent",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<2, sizeof(const void*)>>,
     nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CFTypeOutputArg<2>>},
    {"CFDictionaryReplaceValue", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>, MM_CFTypeArg<1>, MM_CFTypeArg<2>>},
    {"CFEqual", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>>},
    {"CFGetTypeID", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CFLocaleCopyCurrent", RR_ScalarRval},
    {"CFLocaleCopyPreferredLanguages", RR_ScalarRval},
    {"CFLocaleCreate", RR_ScalarRval},
    {"CFLocaleGetIdentifier", RR_ScalarRval},
    {"CFNotificationCenterAddObserver", nullptr,
     Preamble_CFNotificationCenterAddObserver},
    {"CFNotificationCenterGetLocalCenter", RR_ScalarRval},
    {"CFNotificationCenterRemoveObserver"},
    {"CFNumberCreate", RR_ScalarRval, nullptr, MM_CFNumberCreate},
    {"CFNumberGetValue", RR_Compose<RR_ScalarRval, RR_CFNumberGetValue>,
     nullptr, MM_CFNumberGetValue},
    {"CFNumberIsFloatType", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CFPreferencesAppValueIsForced", RR_ScalarRval},
    {"CFPreferencesCopyAppValue", RR_ScalarRval},
    {"CFPreferencesCopyValue", RR_ScalarRval},
    {"CFPropertyListCreateFromXMLData", RR_ScalarRval},
    {"CFPropertyListCreateWithStream", RR_ScalarRval},
    {"CFReadStreamClose"},
    {"CFReadStreamCreateWithFile", RR_ScalarRval},
    {"CFReadStreamOpen", RR_ScalarRval},

    // Don't handle release/retain calls explicitly in the middleman:
    // all resources will be cleaned up when its calls are reset.
    {"CFRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CFRetain", RR_ScalarRval, nullptr, nullptr, MiddlemanPreamble_CFRetain},

    {"CFRunLoopAddSource"},
    {"CFRunLoopGetCurrent", RR_ScalarRval},
    {"CFRunLoopRemoveSource"},
    {"CFRunLoopSourceCreate", RR_ScalarRval, Preamble_CFRunLoopSourceCreate},
    {"CFRunLoopSourceInvalidate"},
    {"CFRunLoopSourceSignal"},
    {"CFRunLoopWakeUp"},
    {"CFStringAppendCharacters"},
    {"CFStringCompare", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>>},
    {"CFStringCreateArrayBySeparatingStrings", RR_ScalarRval},
    {"CFStringCreateMutable", RR_ScalarRval},
    {"CFStringCreateWithBytes", RR_ScalarRval, nullptr,
     MM_Compose<MM_Buffer<1, 2>, MM_CreateCFTypeRval>},
    {"CFStringCreateWithBytesNoCopy", RR_ScalarRval},
    {"CFStringCreateWithCharactersNoCopy", RR_ScalarRval, nullptr,
     MM_Compose<MM_Buffer<1, 2, UniChar>, MM_CreateCFTypeRval>},
    {"CFStringCreateWithCString", RR_ScalarRval},
    {"CFStringCreateWithFormat", RR_ScalarRval},
    {"CFStringGetBytes",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     RR_Compose<RR_ScalarRval, RR_WriteOptionalBuffer<6, 7>,
                RR_WriteOptionalBufferFixedSize<8, sizeof(CFIndex)>>},
    {"CFStringGetCharacters",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     // We also need to specify the argument register with the range's length
     // here.
     RR_WriteBuffer<3, 2, UniChar>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_WriteBuffer<3, 2, UniChar>>},
    {"CFStringGetCString", RR_Compose<RR_ScalarRval, RR_WriteBuffer<1, 2>>},
    {"CFStringGetCStringPtr", nullptr, Preamble_VetoIfNotPassedThrough<0>},
    {"CFStringGetIntValue", RR_ScalarRval},
    {"CFStringGetLength", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CFStringGetMaximumSizeForEncoding", RR_ScalarRval},
    {"CFStringHasPrefix", RR_ScalarRval},
    {"CFStringTokenizerAdvanceToNextToken", RR_ScalarRval},
    {"CFStringTokenizerCreate", RR_ScalarRval},
    {"CFStringTokenizerGetCurrentTokenRange", RR_ComplexScalarRval},
    {"CFURLCreateFromFileSystemRepresentation", RR_ScalarRval},
    {"CFURLCreateFromFSRef", RR_ScalarRval},
    {"CFURLCreateWithFileSystemPath", RR_ScalarRval},
    {"CFURLCreateWithString", RR_ScalarRval},
    {"CFURLGetFileSystemRepresentation",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3>>},
    {"CFURLGetFSRef",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<1, sizeof(FSRef)>>},
    {"CFUUIDCreate", RR_ScalarRval, nullptr, MM_CreateCFTypeRval},
    {"CFUUIDCreateString", RR_ScalarRval},
    {"CFUUIDGetUUIDBytes", RR_ComplexScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGAffineTransformConcat", RR_OversizeRval<sizeof(CGAffineTransform)>},
    {"CGBitmapContextCreateImage", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGBitmapContextCreateWithData",
     RR_Compose<RR_ScalarRval, RR_CGBitmapContextCreateWithData>, nullptr,
     MM_CGBitmapContextCreateWithData},
    {"CGBitmapContextGetBytesPerRow", RR_ScalarRval},
    {"CGBitmapContextGetHeight", RR_ScalarRval},
    {"CGBitmapContextGetWidth", RR_ScalarRval},
    {"CGColorRelease", RR_ScalarRval},
    {"CGColorSpaceCopyICCProfile", RR_ScalarRval},
    {"CGColorSpaceCreateDeviceGray", RR_ScalarRval, nullptr,
     MM_CreateCFTypeRval},
    {"CGColorSpaceCreateDeviceRGB", RR_ScalarRval, nullptr,
     MM_CreateCFTypeRval},
    {"CGColorSpaceCreatePattern", RR_ScalarRval},
    {"CGColorSpaceRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CGContextAddPath", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>>},
    {"CGContextBeginTransparencyLayerWithRect", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_StackArgumentData<sizeof(CGRect)>,
                MM_CFTypeArg<1>>},
    {"CGContextClipToRect", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_StackArgumentData<sizeof(CGRect)>>},
    {"CGContextClipToRects", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 2, CGRect>>},
    {"CGContextConcatCTM", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>,
                MM_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextDrawImage", RR_FlushCGContext<0>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_StackArgumentData<sizeof(CGRect)>,
                MM_CFTypeArg<1>, MM_FlushCGContext<0>>},
    {"CGContextDrawLinearGradient", RR_FlushCGContext<0>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>,
                MM_StackArgumentData<2 * sizeof(CGPoint)>,
                MM_FlushCGContext<0>>},
    {"CGContextEndTransparencyLayer", nullptr, nullptr, MM_CFTypeArg<0>},
    {"CGContextFillPath", RR_FlushCGContext<0>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_FlushCGContext<0>>},
    {"CGContextFillRect", RR_FlushCGContext<0>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_StackArgumentData<sizeof(CGRect)>,
                MM_FlushCGContext<0>>},
    {"CGContextGetClipBoundingBox", RR_OversizeRval<sizeof(CGRect)>},
    {"CGContextGetCTM", RR_OversizeRval<sizeof(CGAffineTransform)>},
    {"CGContextGetType", RR_ScalarRval},
    {"CGContextGetUserSpaceToDeviceSpaceTransform",
     RR_OversizeRval<sizeof(CGAffineTransform)>, nullptr,
     MM_Compose<MM_CFTypeArg<1>, MM_OversizeRval<sizeof(CGAffineTransform)>>},
    {"CGContextRestoreGState", nullptr, Preamble_CGContextRestoreGState,
     MM_UpdateCFTypeArg<0>},
    {"CGContextRotateCTM", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSaveGState", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetAllowsFontSubpixelPositioning", nullptr, nullptr,
     MM_UpdateCFTypeArg<0>},
    {"CGContextSetAllowsFontSubpixelQuantization", nullptr, nullptr,
     MM_UpdateCFTypeArg<0>},
    {"CGContextSetAlpha", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetBaseCTM", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>,
                MM_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextSetCTM", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>,
                MM_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextSetGrayFillColor", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetRGBFillColor", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>, MM_StackArgumentData<sizeof(CGFloat)>>},
    {"CGContextSetRGBStrokeColor", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>, MM_StackArgumentData<sizeof(CGFloat)>>},
    {"CGContextSetShouldAntialias", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetShouldSmoothFonts", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetShouldSubpixelPositionFonts", nullptr, nullptr,
     MM_UpdateCFTypeArg<0>},
    {"CGContextSetShouldSubpixelQuantizeFonts", nullptr, nullptr,
     MM_UpdateCFTypeArg<0>},
    {"CGContextSetTextDrawingMode", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextSetTextMatrix", nullptr, nullptr,
     MM_Compose<MM_UpdateCFTypeArg<0>,
                MM_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextScaleCTM", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGContextStrokeLineSegments", RR_FlushCGContext<0>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 2, CGPoint>,
                MM_FlushCGContext<0>>},
    {"CGContextTranslateCTM", nullptr, nullptr, MM_UpdateCFTypeArg<0>},
    {"CGDataProviderCreateWithData",
     RR_Compose<RR_ScalarRval, RR_CGDataProviderCreateWithData>, nullptr,
     MM_CGDataProviderCreateWithData},
    {"CGDataProviderRelease", nullptr, nullptr, nullptr, Preamble_Veto<0>},
    {"CGDisplayCopyColorSpace", RR_ScalarRval},
    {"CGDisplayIOServicePort", RR_ScalarRval},
    {"CGEventSourceCounterForEventType", RR_ScalarRval},
    {"CGFontCopyTableForTag", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGFontCopyTableTags", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGFontCopyVariations", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGFontCreateCopyWithVariations", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CreateCFTypeRval>},
    {"CGFontCreateWithDataProvider", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGFontCreateWithFontName", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CGFontCreateWithPlatformFont", RR_ScalarRval},
    {"CGFontGetAscent", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGFontGetCapHeight", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGFontGetDescent", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGFontGetFontBBox", RR_OversizeRval<sizeof(CGRect)>, nullptr,
     MM_Compose<MM_CFTypeArg<1>, MM_OversizeRval<sizeof(CGRect)>>},
    {"CGFontGetGlyphAdvances",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, int>>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 2, CGGlyph>,
                MM_WriteBuffer<3, 2, int>>},
    {"CGFontGetGlyphBBoxes",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, CGRect>>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 2, CGGlyph>,
                MM_WriteBuffer<3, 2, CGRect>>},
    {"CGFontGetGlyphPath", RR_ScalarRval},
    {"CGFontGetLeading", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGFontGetUnitsPerEm", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGFontGetXHeight", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CGGradientCreateWithColorComponents", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 3, CGFloat>,
                MM_Buffer<2, 3, CGFloat>, MM_CreateCFTypeRval>},
    {"CGImageGetHeight", RR_ScalarRval},
    {"CGImageGetWidth", RR_ScalarRval},
    {"CGImageRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CGMainDisplayID", RR_ScalarRval},
    {"CGPathAddPath"},
    {"CGPathApply", nullptr, Preamble_CGPathApply},
    {"CGPathContainsPoint", RR_ScalarRval},
    {"CGPathCreateMutable", RR_ScalarRval},
    {"CGPathCreateWithRoundedRect", RR_ScalarRval, nullptr,
     MM_Compose<MM_StackArgumentData<sizeof(CGRect)>,
                MM_BufferFixedSize<0, sizeof(CGAffineTransform)>,
                MM_CreateCFTypeRval>},
    {"CGPathGetBoundingBox", RR_OversizeRval<sizeof(CGRect)>},
    {"CGPathGetCurrentPoint", RR_ComplexFloatRval},
    {"CGPathIsEmpty", RR_ScalarRval},
    {"CGSSetDebugOptions", RR_ScalarRval},
    {"CGSShutdownServerConnections"},
    {"CTFontCopyFamilyName", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCopyFeatures", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCopyFontDescriptor", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCopyGraphicsFont", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCopyTable", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCopyVariationAxes", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontCreateForString", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CreateCFTypeRval>},
    {"CTFontCreatePathForGlyph", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>,
                MM_BufferFixedSize<2, sizeof(CGAffineTransform)>,
                MM_CreateCFTypeRval>},
    {"CTFontCreateWithFontDescriptor", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>,
                MM_BufferFixedSize<1, sizeof(CGAffineTransform)>,
                MM_CreateCFTypeRval>},
    {"CTFontCreateWithGraphicsFont", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>,
                MM_BufferFixedSize<1, sizeof(CGAffineTransform)>,
                MM_CFTypeArg<2>, MM_CreateCFTypeRval>},
    {"CTFontCreateWithName", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>,
                MM_BufferFixedSize<1, sizeof(CGAffineTransform)>,
                MM_CreateCFTypeRval>},
    {"CTFontDescriptorCopyAttribute", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CreateCFTypeRval>},
    {"CTFontDescriptorCreateCopyWithAttributes", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CreateCFTypeRval>},
    {"CTFontDescriptorCreateMatchingFontDescriptors", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CreateCFTypeRval>},
    {"CTFontDescriptorCreateWithAttributes", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTFontDrawGlyphs", RR_FlushCGContext<4>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<4>, MM_Buffer<1, 3, CGGlyph>,
                MM_Buffer<2, 3, CGPoint>, MM_FlushCGContext<4>>},
    {"CTFontGetAdvancesForGlyphs",
     RR_Compose<RR_FloatRval, RR_WriteOptionalBuffer<3, 4, CGSize>>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<2, 4, CGGlyph>,
                MM_WriteBuffer<3, 4, CGSize>>},
    {"CTFontGetAscent", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetBoundingBox", RR_OversizeRval<sizeof(CGRect)>, nullptr,
     MM_Compose<MM_CFTypeArg<1>, MM_OversizeRval<sizeof(CGRect)>>},
    {"CTFontGetBoundingRectsForGlyphs",
     // Argument indexes here are off by one due to the oversize rval.
     RR_Compose<RR_OversizeRval<sizeof(CGRect)>,
                RR_WriteOptionalBuffer<4, 5, CGRect>>,
     nullptr,
     MM_Compose<MM_CFTypeArg<1>, MM_Buffer<3, 5, CGGlyph>,
                MM_OversizeRval<sizeof(CGRect)>, MM_WriteBuffer<4, 5, CGRect>>},
    {"CTFontGetCapHeight", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetDescent", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetGlyphCount", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetGlyphsForCharacters",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3, CGGlyph>>, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_Buffer<1, 3, UniChar>,
                MM_WriteBuffer<2, 3, CGGlyph>>},
    {"CTFontGetLeading", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetSize", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetSymbolicTraits", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetUnderlinePosition", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetUnderlineThickness", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetUnitsPerEm", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontGetXHeight", RR_FloatRval, nullptr, MM_CFTypeArg<0>},
    {"CTFontManagerCopyAvailableFontFamilyNames", RR_ScalarRval},
    {"CTFontManagerRegisterFontsForURLs", RR_ScalarRval},
    {"CTFontManagerSetAutoActivationSetting"},
    {"CTLineCreateWithAttributedString", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CreateCFTypeRval>},
    {"CTLineGetGlyphRuns", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeRval>},
    {"CTRunGetAttributes", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeRval>},
    {"CTRunGetGlyphCount", RR_ScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CTRunGetGlyphsPtr", RR_CTRunGetElements<CGGlyph, CTRunGetGlyphs>, nullptr,
     MM_CTRunGetElements<CGGlyph, CTRunGetGlyphs>},
    {"CTRunGetPositionsPtr", RR_CTRunGetElements<CGPoint, CTRunGetPositions>,
     nullptr, MM_CTRunGetElements<CGPoint, CTRunGetPositions>},
    {"CTRunGetStringIndicesPtr",
     RR_CTRunGetElements<CFIndex, CTRunGetStringIndices>, nullptr,
     MM_CTRunGetElements<CFIndex, CTRunGetStringIndices>},
    {"CTRunGetStringRange", RR_ComplexScalarRval, nullptr, MM_CFTypeArg<0>},
    {"CTRunGetTypographicBounds",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     RR_Compose<RR_FloatRval,
                RR_WriteOptionalBufferFixedSize<3, sizeof(CGFloat)>,
                RR_WriteOptionalBufferFixedSize<4, sizeof(CGFloat)>,
                RR_WriteOptionalBufferFixedSize<5, sizeof(CGFloat)>>,
     nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_WriteBufferFixedSize<3, sizeof(CGFloat)>,
                MM_WriteBufferFixedSize<4, sizeof(CGFloat)>,
                MM_WriteBufferFixedSize<5, sizeof(CGFloat)>>},
    {"CUIDraw", nullptr, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeArg<1>, MM_CFTypeArg<2>,
                MM_StackArgumentData<sizeof(CGRect)>>},
    {"FSCompareFSRefs", RR_ScalarRval},
    {"FSGetVolumeInfo",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<5, sizeof(HFSUniStr255)>,
                RR_WriteBufferFixedSize<6, sizeof(FSRef)>>},
    {"FSFindFolder",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<3, sizeof(FSRef)>>},
    {"Gestalt",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<1, sizeof(SInt32)>>},
    {"GetEventClass", RR_ScalarRval},
    {"GetCurrentEventQueue", RR_ScalarRval},
    {"GetCurrentProcess",
     RR_Compose<RR_ScalarRval,
                RR_WriteBufferFixedSize<0, sizeof(ProcessSerialNumber)>>},
    {"GetEventAttributes", RR_ScalarRval},
    {"GetEventDispatcherTarget", RR_ScalarRval},
    {"GetEventKind", RR_ScalarRval},
    {"HIThemeDrawButton",
     RR_Compose<RR_WriteBufferFixedSize<4, sizeof(HIRect)>, RR_ScalarRval>,
     nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeButtonDrawInfo)>,
                MM_UpdateCFTypeArg<2>,
                MM_WriteBufferFixedSize<4, sizeof(HIRect)>>},
    {"HIThemeDrawFrame", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeFrameDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawGroupBox", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeGroupBoxDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawGrowBox", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIPoint)>,
                MM_BufferFixedSize<1, sizeof(HIThemeGrowBoxDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawMenuBackground", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeMenuDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawMenuItem",
     RR_Compose<RR_WriteBufferFixedSize<5, sizeof(HIRect)>, RR_ScalarRval>,
     nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIRect)>,
                MM_BufferFixedSize<2, sizeof(HIThemeMenuItemDrawInfo)>,
                MM_UpdateCFTypeArg<3>,
                MM_WriteBufferFixedSize<5, sizeof(HIRect)>>},
    {"HIThemeDrawMenuSeparator", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIRect)>,
                MM_BufferFixedSize<2, sizeof(HIThemeMenuItemDrawInfo)>,
                MM_UpdateCFTypeArg<3>>},
    {"HIThemeDrawSeparator", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeSeparatorDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawTabPane", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIRect)>,
                MM_BufferFixedSize<1, sizeof(HIThemeTabPaneDrawInfo)>,
                MM_UpdateCFTypeArg<2>>},
    {"HIThemeDrawTrack", RR_ScalarRval, nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIThemeTrackDrawInfo)>,
                MM_BufferFixedSize<1, sizeof(HIRect)>, MM_UpdateCFTypeArg<2>>},
    {"HIThemeGetGrowBoxBounds",
     RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<2, sizeof(HIRect)>>,
     nullptr,
     MM_Compose<MM_BufferFixedSize<0, sizeof(HIPoint)>,
                MM_BufferFixedSize<1, sizeof(HIThemeGrowBoxDrawInfo)>,
                MM_WriteBufferFixedSize<2, sizeof(HIRect)>>},
    {"HIThemeSetFill", RR_ScalarRval, nullptr, MM_UpdateCFTypeArg<2>},
    {"IORegistryEntrySearchCFProperty", RR_ScalarRval},
    {"LSCopyAllHandlersForURLScheme", RR_ScalarRval},
    {"LSCopyApplicationForMIMEType",
     RR_Compose<RR_ScalarRval,
                RR_WriteOptionalBufferFixedSize<2, sizeof(CFURLRef)>>},
    {"LSCopyItemAttribute",
     RR_Compose<RR_ScalarRval,
                RR_WriteOptionalBufferFixedSize<3, sizeof(CFTypeRef)>>},
    {"LSCopyKindStringForMIMEType",
     RR_Compose<RR_ScalarRval,
                RR_WriteOptionalBufferFixedSize<1, sizeof(CFStringRef)>>},
    {"LSGetApplicationForInfo",
     RR_Compose<RR_ScalarRval,
                RR_WriteOptionalBufferFixedSize<4, sizeof(FSRef)>,
                RR_WriteOptionalBufferFixedSize<5, sizeof(CFURLRef)>>},
    {"LSGetApplicationForURL",
     RR_Compose<RR_ScalarRval,
                RR_WriteOptionalBufferFixedSize<2, sizeof(FSRef)>,
                RR_WriteOptionalBufferFixedSize<3, sizeof(CFURLRef)>>},
    {"NSClassFromString", RR_ScalarRval, nullptr,
     MM_Compose<MM_CFTypeArg<0>, MM_CFTypeRval>},
    {"NSRectFill", nullptr, nullptr, MM_NoOp},
    {"NSSearchPathForDirectoriesInDomains", RR_ScalarRval},
    {"NSSetFocusRingStyle", nullptr, nullptr, MM_NoOp},
    {"NSTemporaryDirectory", RR_ScalarRval},
    {"OSSpinLockLock", nullptr, Preamble_OSSpinLockLock},
    {"ReleaseEvent", RR_ScalarRval},
    {"RemoveEventFromQueue", RR_ScalarRval},
    {"RetainEvent", RR_ScalarRval},
    {"SCDynamicStoreCopyProxies", RR_ScalarRval},
    {"SCDynamicStoreCreate", RR_ScalarRval},
    {"SCDynamicStoreCreateRunLoopSource", RR_ScalarRval},
    {"SCDynamicStoreKeyCreateProxies", RR_ScalarRval},
    {"SCDynamicStoreSetNotificationKeys", RR_ScalarRval},
    {"SendEventToEventTarget", RR_ScalarRval},

    // These are not public APIs, but other redirected functions may be aliases
    // for these which are dynamically installed on the first call in a way that
    // our redirection mechanism doesn't completely account for.
    {"SLDisplayCopyColorSpace", RR_ScalarRval},
    {"SLDisplayIOServicePort", RR_ScalarRval},
    {"SLEventSourceCounterForEventType", RR_ScalarRval},
    {"SLMainDisplayID", RR_ScalarRval},
    {"SLSSetDenyWindowServerConnections", RR_ScalarRval},
    {"SLSShutdownServerConnections"},
};

///////////////////////////////////////////////////////////////////////////////
// Diagnostic Redirections
///////////////////////////////////////////////////////////////////////////////

// Diagnostic redirections are used to redirection functions in Gecko in order
// to help hunt down the reasons for crashes or other failures. Because of the
// unpredictable effects of inlining, diagnostic redirections may not be called
// whenever the associated function is invoked, and should not be used to
// modify Gecko's behavior.
//
// The address of the function to redirect is specified directly here, as we
// cannot use dlsym() to lookup symbols that are not externally visible.

// Functions which are not overloaded.
#define FOR_EACH_DIAGNOSTIC_REDIRECTION(MACRO)              \
  MACRO(PL_HashTableAdd, Preamble_PLHashTable)              \
  MACRO(PL_HashTableRemove, Preamble_PLHashTable)           \
  MACRO(PL_HashTableLookup, Preamble_PLHashTable)           \
  MACRO(PL_HashTableLookupConst, Preamble_PLHashTable)      \
  MACRO(PL_HashTableEnumerateEntries, Preamble_PLHashTable) \
  MACRO(PL_HashTableRawAdd, Preamble_PLHashTable)           \
  MACRO(PL_HashTableRawRemove, Preamble_PLHashTable)        \
  MACRO(PL_HashTableRawLookup, Preamble_PLHashTable)        \
  MACRO(PL_HashTableRawLookupConst, Preamble_PLHashTable)

// Member functions which need a type specification to resolve overloading.
#define FOR_EACH_DIAGNOSTIC_MEMBER_PTR_WITH_TYPE_REDIRECTION(MACRO)         \
  MACRO(PLDHashEntryHdr* (PLDHashTable::*)(const void*, const fallible_t&), \
        &PLDHashTable::Add, Preamble_PLDHashTable)

// Member functions which are not overloaded.
#define FOR_EACH_DIAGNOSTIC_MEMBER_PTR_REDIRECTION(MACRO) \
  MACRO(&PLDHashTable::Clear, Preamble_PLDHashTable)      \
  MACRO(&PLDHashTable::Remove, Preamble_PLDHashTable)     \
  MACRO(&PLDHashTable::RemoveEntry, Preamble_PLDHashTable)

static PreambleResult Preamble_PLHashTable(CallArguments* aArguments) {
  CheckPLHashTable(aArguments->Arg<0, PLHashTable*>());
  return PreambleResult::IgnoreRedirect;
}

static PreambleResult Preamble_PLDHashTable(CallArguments* aArguments) {
  CheckPLDHashTable(aArguments->Arg<0, PLDHashTable*>());
  return PreambleResult::IgnoreRedirect;
}

#define MAKE_DIAGNOSTIC_ENTRY_WITH_TYPE(aType, aAddress, aPreamble) \
  {#aAddress, nullptr, nullptr, nullptr, aPreamble},

#define MAKE_DIAGNOSTIC_ENTRY(aAddress, aPreamble) \
  {#aAddress, nullptr, nullptr, nullptr, aPreamble},

static Redirection gDiagnosticRedirections[] = {
    // clang-format off
  FOR_EACH_DIAGNOSTIC_REDIRECTION(MAKE_DIAGNOSTIC_ENTRY)
  FOR_EACH_DIAGNOSTIC_MEMBER_PTR_WITH_TYPE_REDIRECTION(MAKE_DIAGNOSTIC_ENTRY_WITH_TYPE)
  FOR_EACH_DIAGNOSTIC_MEMBER_PTR_REDIRECTION(MAKE_DIAGNOSTIC_ENTRY)
    // clang-format on
};

#undef MAKE_DIAGNOSTIC_ENTRY_WITH_TYPE
#undef MAKE_DIAGNOSTIC_ENTRY

///////////////////////////////////////////////////////////////////////////////
// Redirection generation
///////////////////////////////////////////////////////////////////////////////

size_t NumRedirections() {
  return ArrayLength(gSystemRedirections) +
         ArrayLength(gDiagnosticRedirections);
}

static Redirection* gRedirections;

Redirection& GetRedirection(size_t aCallId) {
  if (aCallId < ArrayLength(gSystemRedirections)) {
    return gRedirections[aCallId];
  }
  aCallId -= ArrayLength(gSystemRedirections);
  MOZ_RELEASE_ASSERT(aCallId < ArrayLength(gDiagnosticRedirections));
  return gDiagnosticRedirections[aCallId];
}

// Get the instruction pointer to use as the address of the base function for a
// redirection.
static uint8_t* FunctionStartAddress(Redirection& aRedirection) {
  uint8_t* addr =
      static_cast<uint8_t*>(dlsym(RTLD_DEFAULT, aRedirection.mName));
  if (!addr) return nullptr;

  if (addr[0] == 0xFF && addr[1] == 0x25) {
    return *(uint8_t**)(addr + 6 + *reinterpret_cast<int32_t*>(addr + 2));
  }

  return addr;
}

template <typename FnPtr>
static uint8_t* ConvertMemberPtrToAddress(FnPtr aPtr) {
  // Dig around in clang's internal representation of member function pointers.
  uint8_t** contents = (uint8_t**)&aPtr;
  return contents[0];
}

void EarlyInitializeRedirections() {
  size_t numSystemRedirections = ArrayLength(gSystemRedirections);
  gRedirections = new Redirection[numSystemRedirections];
  PodZero(gRedirections, numSystemRedirections);

  for (size_t i = 0; i < numSystemRedirections; i++) {
    const SystemRedirection& systemRedirection = gSystemRedirections[i];
    Redirection& redirection = gRedirections[i];

    redirection.mName = systemRedirection.mName;
    redirection.mSaveOutput = systemRedirection.mSaveOutput;
    redirection.mPreamble = systemRedirection.mPreamble;
    redirection.mMiddlemanCall = systemRedirection.mMiddlemanCall;
    redirection.mMiddlemanPreamble = systemRedirection.mMiddlemanPreamble;

    redirection.mBaseFunction = FunctionStartAddress(redirection);
    redirection.mOriginalFunction = redirection.mBaseFunction;

    if (redirection.mBaseFunction && IsRecordingOrReplaying()) {
      // We will get confused if we try to redirect the same address in multiple
      // places.
      for (size_t j = 0; j < i; j++) {
        if (gRedirections[j].mBaseFunction == redirection.mBaseFunction) {
          redirection.mBaseFunction = nullptr;
          break;
        }
      }
    }
  }

  size_t diagnosticIndex = 0;

#define LOAD_DIAGNOSTIC_ENTRY(aAddress, aPreamble)           \
  gDiagnosticRedirections[diagnosticIndex++].mBaseFunction = \
      BitwiseCast<uint8_t*>(aAddress);
  FOR_EACH_DIAGNOSTIC_REDIRECTION(LOAD_DIAGNOSTIC_ENTRY)
#undef LOAD_DIAGNOSTIC_ENTRY

#define LOAD_DIAGNOSTIC_ENTRY(aType, aAddress, aPreamble)    \
  gDiagnosticRedirections[diagnosticIndex++].mBaseFunction = \
      ConvertMemberPtrToAddress<aType>(aAddress);
  FOR_EACH_DIAGNOSTIC_MEMBER_PTR_WITH_TYPE_REDIRECTION(LOAD_DIAGNOSTIC_ENTRY)
#undef LOAD_DIAGNOSTIC_ENTRY

#define LOAD_DIAGNOSTIC_ENTRY(aAddress, aPreamble)           \
  gDiagnosticRedirections[diagnosticIndex++].mBaseFunction = \
      ConvertMemberPtrToAddress(aAddress);
  FOR_EACH_DIAGNOSTIC_MEMBER_PTR_REDIRECTION(LOAD_DIAGNOSTIC_ENTRY)
#undef LOAD_DIAGNOSTIC_ENTRY

  for (Redirection& redirection : gDiagnosticRedirections) {
    redirection.mOriginalFunction = redirection.mBaseFunction;
  }

  // Bind the gOriginal functions to their redirections' base addresses until we
  // finish installing redirections.
  LateInitializeRedirections();

  InitializeStaticClasses();
}

void LateInitializeRedirections() {
#define INIT_ORIGINAL_FUNCTION(aName) \
  gOriginal_##aName = OriginalFunction(#aName);

  FOR_EACH_ORIGINAL_FUNCTION(INIT_ORIGINAL_FUNCTION)

#undef INIT_ORIGINAL_FUNCTION
}

///////////////////////////////////////////////////////////////////////////////
// Direct system call API
///////////////////////////////////////////////////////////////////////////////

const char* SymbolNameRaw(void* aPtr) {
  Dl_info info;
  return (dladdr(aPtr, &info) && info.dli_sname) ? info.dli_sname : "???";
}

void* DirectAllocateMemory(void* aAddress, size_t aSize) {
  void* res = CallFunction<void*>(gOriginal_mmap, aAddress, aSize,
                                  PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_ANON | MAP_PRIVATE, -1, 0);
  MOZ_RELEASE_ASSERT(res && res != (void*)-1);
  return res;
}

void DirectDeallocateMemory(void* aAddress, size_t aSize) {
  ssize_t rv = CallFunction<int>(gOriginal_munmap, aAddress, aSize);
  MOZ_RELEASE_ASSERT(rv >= 0);
}

void DirectWriteProtectMemory(void* aAddress, size_t aSize, bool aExecutable,
                              bool aIgnoreFailures /* = false */) {
  ssize_t rv = CallFunction<int>(gOriginal_mprotect, aAddress, aSize,
                                 PROT_READ | (aExecutable ? PROT_EXEC : 0));
  MOZ_RELEASE_ASSERT(aIgnoreFailures || rv == 0);
}

void DirectUnprotectMemory(void* aAddress, size_t aSize, bool aExecutable,
                           bool aIgnoreFailures /* = false */) {
  ssize_t rv =
      CallFunction<int>(gOriginal_mprotect, aAddress, aSize,
                        PROT_READ | PROT_WRITE | (aExecutable ? PROT_EXEC : 0));
  MOZ_RELEASE_ASSERT(aIgnoreFailures || rv == 0);
}

void DirectSeekFile(FileHandle aFd, uint64_t aOffset) {
  static_assert(sizeof(uint64_t) == sizeof(off_t), "off_t should have 64 bits");
  ssize_t rv =
      HANDLE_EINTR(CallFunction<int>(gOriginal_lseek, aFd, aOffset, SEEK_SET));
  MOZ_RELEASE_ASSERT(rv >= 0);
}

FileHandle DirectOpenFile(const char* aFilename, bool aWriting) {
  int flags = aWriting ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY;
  int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd =
      HANDLE_EINTR(CallFunction<int>(gOriginal_open, aFilename, flags, perms));
  MOZ_RELEASE_ASSERT(fd > 0);
  return fd;
}

void DirectCloseFile(FileHandle aFd) {
  ssize_t rv = HANDLE_EINTR(CallFunction<int>(gOriginal_close, aFd));
  MOZ_RELEASE_ASSERT(rv >= 0);
}

void DirectDeleteFile(const char* aFilename) {
  ssize_t rv = unlink(aFilename);
  MOZ_RELEASE_ASSERT(rv >= 0 || errno == ENOENT);
}

void DirectWrite(FileHandle aFd, const void* aData, size_t aSize) {
  ssize_t rv =
      HANDLE_EINTR(CallFunction<int>(gOriginal_write, aFd, aData, aSize));
  MOZ_RELEASE_ASSERT((size_t)rv == aSize);
}

void DirectPrint(const char* aString) {
  DirectWrite(STDERR_FILENO, aString, strlen(aString));
}

size_t DirectRead(FileHandle aFd, void* aData, size_t aSize) {
  // Clear the memory in case it is write protected by the memory snapshot
  // mechanism.
  memset(aData, 0, aSize);
  ssize_t rv =
      HANDLE_EINTR(CallFunction<int>(gOriginal_read, aFd, aData, aSize));
  MOZ_RELEASE_ASSERT(rv >= 0);
  return (size_t)rv;
}

void DirectCreatePipe(FileHandle* aWriteFd, FileHandle* aReadFd) {
  int fds[2];
  ssize_t rv = CallFunction<int>(gOriginal_pipe, fds);
  MOZ_RELEASE_ASSERT(rv >= 0);
  *aWriteFd = fds[1];
  *aReadFd = fds[0];
}

static double gAbsoluteToNanosecondsRate;

void InitializeCurrentTime() {
  AbsoluteTime time = {1000000, 0};
  Nanoseconds rate = AbsoluteToNanoseconds(time);
  MOZ_RELEASE_ASSERT(!rate.hi);
  gAbsoluteToNanosecondsRate = rate.lo / 1000000.0;
}

double CurrentTime() {
  return CallFunction<int64_t>(gOriginal_mach_absolute_time) *
         gAbsoluteToNanosecondsRate / 1000.0;
}

void DirectSpawnThread(void (*aFunction)(void*), void* aArgument) {
  MOZ_RELEASE_ASSERT(IsMiddleman() || AreThreadEventsPassedThrough());

  pthread_attr_t attr;
  int rv = pthread_attr_init(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  MOZ_RELEASE_ASSERT(rv == 0);

  pthread_t pthread;
  rv = CallFunction<int>(gOriginal_pthread_create, &pthread, &attr,
                         (void* (*)(void*))aFunction, aArgument);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_destroy(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);
}

}  // namespace recordreplay
}  // namespace mozilla
