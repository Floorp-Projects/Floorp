/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRedirect.h"

#include "mozilla/Maybe.h"

#include "HashTable.h"
#include "Lock.h"
#include "ProcessRecordReplay.h"
#include "ProcessRewind.h"
#include "base/eintr_wrapper.h"

#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <signal.h>
#include <sys/attr.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/utsname.h>
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
  MACRO(close)                              \
  MACRO(lseek)                              \
  MACRO(mach_absolute_time)                 \
  MACRO(mmap)                               \
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
  MACRO(pthread_self)                       \
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
};

typedef void (*CFRunLoopPerformCallBack)(void*);

static void CFRunLoopPerformCallBackWrapper(void* aInfo) {
  RecordReplayCallback(CFRunLoopPerformCallBack, &aInfo);
  rrc.mFunction(aInfo);

  // Make sure we service any callbacks that have been posted for the main
  // thread whenever the main thread's message loop has any activity.
  PauseMainThreadAndServiceCallbacks();
}

void ReplayInvokeCallback(size_t aCallbackId) {
  MOZ_RELEASE_ASSERT(IsReplaying());
  switch (aCallbackId) {
    case CallbackEvent_CFRunLoopPerformCallBack:
      CFRunLoopPerformCallBackWrapper(nullptr);
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
static void EX_ObjCInput(ExternalCallContext& aCx, id* aThingPtr) {
  MOZ_RELEASE_ASSERT(aCx.AccessInput());

  if (EX_SystemInput(aCx, (const void**)aThingPtr)) {
    // This value came from a previous middleman call.
    return;
  }

  if (aCx.mPhase == ExternalCallPhase::SaveInput) {
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
    Dl_info info;
    if (dladdr(*aThingPtr, &info) != 0) {
      CFConstantString* str = (CFConstantString*)*aThingPtr;
      if (str->mClass == gCFConstantStringClass &&
          str->mLength <= 4096 &&  // Sanity check.
          dladdr(str->mData, &info) != 0) {
        InfallibleVector<UniChar> buffer;
        aCx.WriteInputScalar((size_t)ObjCInputKind::ConstantString);
        aCx.WriteInputScalar(str->mLength);
        aCx.WriteInputBytes(str->mData, str->mLength);
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
      UniquePtr<char[]> cstring(new char[len]);
      aCx.ReadInputBytes(cstring.get(), len);

      // When replaying in the cloud, external string references are generated
      // on the fly and don't have correct contents. Parse the contents we are
      // given so we can do a dynamic lookup and generate the right string.
      static const char cloudPrefix[] = "RECORD_REPLAY_STRING:";
      if (len >= sizeof(cloudPrefix) &&
          !memcmp(cstring.get(), cloudPrefix, sizeof(cloudPrefix))) {
        MOZ_RELEASE_ASSERT(cstring.get()[len - 1] == 0);
        void* ptr = dlsym(RTLD_DEFAULT, cstring.get() + strlen(cloudPrefix));
        MOZ_RELEASE_ASSERT(ptr);
        *aThingPtr = (id)*(CFStringRef*)ptr;
        break;
      }

      NS_ConvertUTF8toUTF16 converted(cstring.get(), len);
      UniquePtr<UniChar[]> contents(new UniChar[len]);
      memcpy(contents.get(), converted.get(), len * sizeof(UniChar));
      *aThingPtr = (id)CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                    contents.get(), len);
      break;
    }
    default:
      MOZ_CRASH();
  }
}

template <size_t Argument>
static void EX_CFTypeArg(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto& object = aCx.mArguments->Arg<Argument, id>();
    EX_ObjCInput(aCx, &object);
  }
}

static void EX_CFTypeOutput(ExternalCallContext& aCx, CFTypeRef* aOutput,
                            bool aOwnsReference) {
  EX_SystemOutput(aCx, (const void**)aOutput);

  const void* value = *aOutput;
  if (value && aCx.mPhase == ExternalCallPhase::SaveOutput && !IsReplaying()) {
    if (!aOwnsReference) {
      CFRetain(value);
    }
    aCx.mReleaseCallbacks->append([=]() { CFRelease(value); });
  }
}

// For APIs using the 'Get' rule: no reference is held on the returned value.
static void EX_CFTypeRval(ExternalCallContext& aCx) {
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  EX_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ false);
}

// For APIs using the 'Create' rule: a reference is held on the returned
// value which must be released.
static void EX_CreateCFTypeRval(ExternalCallContext& aCx) {
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  EX_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ true);
}

template <size_t Argument>
static void EX_CFTypeOutputArg(ExternalCallContext& aCx) {
  auto& arg = aCx.mArguments->Arg<Argument, const void**>();

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    arg = (const void**) aCx.AllocateBytes(sizeof(const void*));
  }

  EX_CFTypeOutput(aCx, arg, /* aOwnsReference = */ false);
}

static void SendMessageToObject(const void* aObject, const char* aMessage) {
  CallArguments arguments;
  arguments.Arg<0, const void*>() = aObject;
  arguments.Arg<1, SEL>() = sel_registerName(aMessage);
  RecordReplayInvokeCall(gOriginal_objc_msgSend, &arguments);
}

// For APIs whose result will be released by the middleman's autorelease pool.
static void EX_AutoreleaseCFTypeRval(ExternalCallContext& aCx) {
  auto& rvalReference = aCx.mArguments->Rval<const void*>();
  EX_SystemOutput(aCx, &rvalReference);
  const void* rval = rvalReference;

  if (rval && aCx.mPhase == ExternalCallPhase::SaveOutput && !IsReplaying()) {
    SendMessageToObject(rval, "retain");
    aCx.mReleaseCallbacks->append([=]() {
        SendMessageToObject(rval, "autorelease");
      });
  }
}

// For functions which have an input CFType value and also have side effects on
// that value, this associates the call with its own input value so that this
// will be treated as a dependent for any future calls using the value.
template <size_t Argument>
static void EX_UpdateCFTypeArg(ExternalCallContext& aCx) {
  auto arg = aCx.mArguments->Arg<Argument, const void*>();

  EX_CFTypeArg<Argument>(aCx);
  EX_SystemOutput(aCx, &arg, /* aUpdating = */ true);
}

template <int Error = EAGAIN>
static PreambleResult Preamble_SetError(CallArguments* aArguments) {
  aArguments->Rval<ssize_t>() = -1;
  errno = Error;
  return PreambleResult::Veto;
}

#define ForEachFixedInputAddress(Macro)                 \
  Macro(kCFTypeArrayCallBacks)                          \
  Macro(kCFTypeDictionaryKeyCallBacks)                  \
  Macro(kCFTypeDictionaryValueCallBacks)

#define ForEachFixedInput(Macro)                \
  Macro(kCFAllocatorDefault)                    \
  Macro(kCFAllocatorNull)

enum class FixedInput {
#define DefineEnum(Name) Name,
  ForEachFixedInputAddress(DefineEnum)
  ForEachFixedInput(DefineEnum)
#undef DefineEnum
};

static const void* GetFixedInput(FixedInput aWhich) {
  switch (aWhich) {
#define FetchEnumAddress(Name) case FixedInput::Name: return &Name;
    ForEachFixedInputAddress(FetchEnumAddress)
#undef FetchEnumAddress
#define FetchEnum(Name) case FixedInput::Name: return Name;
    ForEachFixedInput(FetchEnum)
#undef FetchEnum
  }
  MOZ_CRASH("Unknown fixed input");
  return nullptr;
}

template <size_t Arg, FixedInput Which>
static void EX_RequireFixed(ExternalCallContext& aCx) {
  auto& arg = aCx.mArguments->Arg<Arg, const void*>();

  if (aCx.AccessInput()) {
    const void* value = GetFixedInput(Which);
    if (aCx.mPhase == ExternalCallPhase::SaveInput) {
      MOZ_RELEASE_ASSERT(arg == value ||
                         (Which == FixedInput::kCFAllocatorDefault &&
                          arg == nullptr));
    } else {
      arg = value;
    }
  }
}

template <size_t Arg>
static void EX_RequireDefaultAllocator(ExternalCallContext& aCx) {
  EX_RequireFixed<0, FixedInput::kCFAllocatorDefault>(aCx);
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

// When recording, this tracks open file descriptors referring to files within
// the installation directory.
struct InstallDirectoryFd {
  size_t mFd;
  UniquePtr<char[]> mSuffix;

  InstallDirectoryFd(size_t aFd, UniquePtr<char[]> aSuffix)
      : mFd(aFd), mSuffix(std::move(aSuffix)) {}
};
static StaticInfallibleVector<InstallDirectoryFd> gInstallDirectoryFds;
static SpinLock gInstallDirectoryFdsLock;

static void RR_open(Stream& aEvents, CallArguments* aArguments,
                    ErrorType* aError) {
  RR_SaveRvalHadErrorNegative(aEvents, aArguments, aError);
  auto fd = aArguments->Rval<ssize_t>();

  // Keep track of which fds refer to a file within the install directory,
  // in case they are mmap'ed later.
  if (IsRecording() && fd >= 0) {
    auto path = aArguments->Arg<0, const char*>();
    const char* installDirectory = InstallDirectory();
    if (installDirectory) {
      size_t installLength = strlen(installDirectory);
      if (!strncmp(installDirectory, path, installLength)) {
        size_t pathLength = strlen(path);
        UniquePtr<char[]> suffix(new char[pathLength - installLength + 1]);
        strcpy(suffix.get(), path + installLength);

        AutoSpinLock lock(gInstallDirectoryFdsLock);
        gInstallDirectoryFds.emplaceBack(fd, std::move(suffix));
      }
    }
  }
}

static void RR_close(Stream& aEvents, CallArguments* aArguments,
                    ErrorType* aError) {
  RR_SaveRvalHadErrorNegative(aEvents, aArguments, aError);

  if (IsRecording()) {
    auto fd = aArguments->Arg<0, size_t>();
    AutoSpinLock lock(gInstallDirectoryFdsLock);
    for (auto& info : gInstallDirectoryFds) {
      if (info.mFd == fd) {
        gInstallDirectoryFds.erase(&info);
        break;
      }
    }
  }
}

static PreambleResult Preamble_mmap(CallArguments* aArguments) {
  auto& address = aArguments->Arg<0, void*>();
  auto& size = aArguments->Arg<1, size_t>();
  auto& prot = aArguments->Arg<2, size_t>();
  auto& flags = aArguments->Arg<3, size_t>();
  auto& fd = aArguments->Arg<4, size_t>();
  auto& offset = aArguments->Arg<5, size_t>();

  MOZ_RELEASE_ASSERT(address == PageBase(address));

  bool mappingFile = !(flags & MAP_ANON) && !AreThreadEventsPassedThrough();
  if (IsReplaying() && mappingFile) {
    flags |= MAP_ANON;
    prot |= PROT_WRITE;
    fd = 0;
    offset = 0;
  }

  if (IsReplaying() && !AreThreadEventsPassedThrough()) {
    flags &= ~MAP_SHARED;
    flags |= MAP_PRIVATE;
  }

  void* memory = CallFunction<void*>(gOriginal_mmap, address, size, prot, flags,
                                     fd, offset);

  if (mappingFile) {
    // When replaying, we need to be able to recover the file contents.
    // If the file descriptor is associated with a file in the install
    // directory, we only need to include its suffix and a hash.
    // Otherwise include all contents of the mapped file.
    MOZ_RELEASE_ASSERT(memory != MAP_FAILED);

    if (IsRecording()) {
      AutoSpinLock lock(gInstallDirectoryFdsLock);
      bool found = false;
      for (const auto& info : gInstallDirectoryFds) {
        if (info.mFd == fd) {
          size_t len = strlen(info.mSuffix.get()) + 1;
          RecordReplayValue(len);
          RecordReplayBytes(info.mSuffix.get(), len);
          RecordReplayValue(HashBytes(memory, size));
          found = true;
          break;
        }
      }
      if (!found) {
        RecordReplayValue(0);
        RecordReplayBytes(memory, size);
      }
    } else {
      size_t len = RecordReplayValue(0);
      if (len) {
        const char* installDirectory = InstallDirectory();
        MOZ_RELEASE_ASSERT(installDirectory);

        size_t installLength = strlen(installDirectory);

        UniquePtr<char[]> path(new char[installLength + len]);
        strcpy(path.get(), installDirectory);
        RecordReplayBytes(path.get() + installLength, len);
        size_t hash = RecordReplayValue(0);

        int fd = DirectOpenFile(path.get(), /* aWriting */ false);
        void* fileContents = CallFunction<void*>(gOriginal_mmap, nullptr, size,
                                                 PROT_READ, MAP_PRIVATE,
                                                 fd, offset);
        MOZ_RELEASE_ASSERT(fileContents != MAP_FAILED);

        memcpy(memory, fileContents, size);
        MOZ_RELEASE_ASSERT(hash == HashBytes(memory, size));

        munmap(fileContents, size);
        DirectCloseFile(fd);
      } else {
        RecordReplayBytes(memory, size);
      }
    }
  }

  aArguments->Rval<void*>() = memory;
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

// Record/replay a variety of sysctl-like functions.
template <size_t BufferArg>
static void RR_sysctl(Stream& aEvents, CallArguments* aArguments,
                      ErrorType* aError) {
  auto& old = aArguments->Arg<BufferArg, void*>();
  auto& oldlenp = aArguments->Arg<BufferArg + 1, size_t*>();

  aEvents.CheckInput((old ? 1 : 0) | (oldlenp ? 2 : 0));
  if (oldlenp) {
    aEvents.RecordOrReplayValue(oldlenp);
  }
  if (old) {
    aEvents.RecordOrReplayBytes(old, *oldlenp);
  }
}

static PreambleResult Preamble_sysctlbyname(CallArguments* aArguments) {
  auto name = aArguments->Arg<0, const char*>();

  // Include the environment variable being checked in an assertion, to make it
  // easier to debug recording mismatches involving sysctlbyname.
  RecordReplayAssert("sysctlbyname %s", name);
  return PreambleResult::Redirect;
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

void DirectLockMutex(pthread_mutex_t* aMutex, bool aPassThroughEvents) {
  Maybe<AutoPassThroughThreadEvents> pt;
  if (aPassThroughEvents) {
    pt.emplace();
  }
  ssize_t rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_lock, aMutex);
  if (rv != 0) {
    Print("CRASH DirectLockMutex %d %d\n", rv, errno);
  }
  MOZ_RELEASE_ASSERT(rv == 0);
}

void DirectUnlockMutex(pthread_mutex_t* aMutex, bool aPassThroughEvents) {
  Maybe<AutoPassThroughThreadEvents> pt;
  if (aPassThroughEvents) {
    pt.emplace();
  }
  ssize_t rv = CallFunction<ssize_t>(gOriginal_pthread_mutex_unlock, aMutex);
  if (rv != 0) {
    Print("CRASH DirectUnlockMutex %d %d\n", rv, errno);
  }
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
      if (thread->MaybeWaitForFork(
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
  lock->Exit(aMutex);
  lock->Enter(aMutex);
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
    AutoEnsurePassThroughThreadEvents pt;
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
    lock->Enter(mutex);
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
    lock->Enter(mutex);
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
    AutoEnsurePassThroughThreadEvents pt;
    aArguments->Rval<ssize_t>() =
        CallFunction<ssize_t>(gOriginal_pthread_mutex_unlock, mutex);
    return PreambleResult::Veto;
  }
  lock->Exit(mutex);
  DirectUnlockMutex(mutex);
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult Preamble_pthread_getspecific(CallArguments* aArguments) {
  if (IsReplaying()) {
    Thread* thread = Thread::Current();
    if (thread && !thread->IsMainThread()) {
      auto key = aArguments->Arg<0, pthread_key_t>();
      void** ptr = thread->GetOrCreateStorage(key);
      aArguments->Rval<void*>() = *ptr;
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::PassThrough;
}

static PreambleResult Preamble_pthread_setspecific(CallArguments* aArguments) {
  if (IsReplaying()) {
    Thread* thread = Thread::Current();
    if (thread && !thread->IsMainThread()) {
      auto key = aArguments->Arg<0, pthread_key_t>();
      auto value = aArguments->Arg<1, void*>();
      void** ptr = thread->GetOrCreateStorage(key);
      *ptr = value;
      aArguments->Rval<ssize_t>() = 0;
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::PassThrough;
}

static PreambleResult Preamble_pthread_self(CallArguments* aArguments) {
  if (IsReplaying() && !AreThreadEventsPassedThrough()) {
    Thread* thread = Thread::Current();
    if (!thread->IsMainThread()) {
      aArguments->Rval<pthread_t>() = thread->NativeId();
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::PassThrough;
}

static void*
GetTLVTemplate(void* aPtr, size_t* aTemplateSize, size_t* aTotalSize) {
  void* tlvTemplate;
  *aTemplateSize = 0;
  *aTotalSize = 0;

  Dl_info info;
  dladdr(aPtr, &info);
  mach_header_64* header = (mach_header_64*) info.dli_fbase;
  MOZ_RELEASE_ASSERT(header->magic == MH_MAGIC_64);

  uint32_t offset = sizeof(mach_header_64);
  for (size_t i = 0; i < header->ncmds; i++) {
    load_command* cmd = (load_command*) ((uint8_t*)header + offset);
    if (LC_SEGMENT_64 == (cmd->cmd & ~LC_REQ_DYLD)) {
      segment_command_64* ncmd = (segment_command_64*) cmd;
      section_64* sect = (section_64*) (ncmd + 1);
      for (size_t i = 0; i < ncmd->nsects; i++, sect++) {
        switch (sect->flags & SECTION_TYPE) {
        case S_THREAD_LOCAL_REGULAR:
          MOZ_RELEASE_ASSERT(!*aTotalSize);
          tlvTemplate = (uint8_t*)header + sect->addr;
          *aTemplateSize += sect->size;
          *aTotalSize += sect->size;
          break;
        case S_THREAD_LOCAL_ZEROFILL:
          *aTotalSize += sect->size;
          break;
        }
      }
    }
    offset += cmd->cmdsize;
  }

  return tlvTemplate;
}

static PreambleResult Preamble__tlv_bootstrap(CallArguments* aArguments) {
  if (IsReplaying()) {
    Thread* thread = Thread::Current();
    if (thread && !thread->IsMainThread()) {
      auto desc = aArguments->Arg<0, tlv_descriptor*>();
      void** ptr = thread->GetOrCreateStorage(desc->key);
      if (!(*ptr)) {
        size_t templateSize, totalSize;
        void* tlvTemplate = GetTLVTemplate(desc, &templateSize, &totalSize);
        MOZ_RELEASE_ASSERT(desc->offset < totalSize);
        void* memory = DirectAllocateMemory(totalSize);
        memcpy(memory, tlvTemplate, templateSize);
        *ptr = memory;
      }
      aArguments->Rval<void*>() = ((uint8_t*)*ptr) + desc->offset;
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::PassThrough;
}

///////////////////////////////////////////////////////////////////////////////
// stdlib redirections
///////////////////////////////////////////////////////////////////////////////

static void* FindRedirectionBinding(const char* aName);

static PreambleResult Preamble_dlopen(CallArguments* aArguments) {
  if (!AreThreadEventsPassedThrough()) {
    auto name = aArguments->Arg<0, const char*>();
    if (name) {
      AutoPassThroughThreadEvents pt;

      static void* fnptr = OriginalFunction("dlopen");
      RecordReplayInvokeCall(fnptr, aArguments);

      void* handle = aArguments->Rval<void*>();
      if (handle) {
        ApplyLibraryRedirections(handle);
      }

      return PreambleResult::Veto;
    }
  }

  return PreambleResult::PassThrough;
}

static PreambleResult Preamble_dlsym(CallArguments* aArguments) {
  auto name = aArguments->Arg<1, const char*>();

  // If a redirected function is dynamically looked up, return the redirection.
  if (!AreThreadEventsPassedThrough()) {
    void* binding = FindRedirectionBinding(name);
    if (binding) {
      aArguments->Rval<void*>() = binding;
      return PreambleResult::Veto;
    }
  }

  return PreambleResult::PassThrough;
}

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

static void RR_host_info_or_statistics(Stream& aEvents,
                                       CallArguments* aArguments,
                                       ErrorType* aError) {
  RR_ScalarRval(aEvents, aArguments, aError);
  auto buf = aArguments->Arg<2, int*>();
  auto size = aArguments->Arg<3, mach_msg_type_number_t*>();

  aEvents.RecordOrReplayValue(size);
  aEvents.RecordOrReplayBytes(buf, *size * sizeof(int));
}

static void RR_readdir(Stream& aEvents, CallArguments* aArguments,
                       ErrorType* aError) {
  auto& rval = aArguments->Rval<dirent*>();
  size_t nbytes = 0;
  if (IsRecording() && rval) {
    nbytes = offsetof(dirent, d_name) + strlen(rval->d_name) + 1;
  }
  aEvents.RecordOrReplayValue(&nbytes);
  if (IsReplaying()) {
    rval = nbytes ? (dirent*)NewLeakyArray<char>(nbytes) : nullptr;
  }
  if (nbytes) {
    aEvents.RecordOrReplayBytes(rval, nbytes);
  }
}

static PreambleResult Preamble_mach_thread_self(CallArguments* aArguments) {
  if (IsReplaying() && !AreThreadEventsPassedThrough()) {
    Thread* thread = Thread::Current();
    if (!thread->IsMainThread()) {
      aArguments->Rval<uintptr_t>() = thread->GetMachId();
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::PassThrough;
}

static void RR_task_threads(Stream& aEvents, CallArguments* aArguments,
                            ErrorType* aError) {
  RR_ScalarRval(aEvents, aArguments, aError);

  auto& buf = aArguments->Arg<1, void**>();
  auto& count = aArguments->Arg<2, mach_msg_type_number_t*>();

  aEvents.RecordOrReplayValue(count);

  if (IsReplaying()) {
    *buf = NewLeakyArray<void*>(*count);
  }
  aEvents.RecordOrReplayBytes(*buf, *count * sizeof(void*));
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

    // Save the length so that EX_NSStringGetCharacters can recover it when
    // saving output while replaying.
    Thread::Current()->mRedirectionValue = len;
  }

  if (!strcmp(message, "UTF8String") ||
      !strcmp(message, "cStringUsingEncoding:")) {
    RR_CStringRval(aEvents, aArguments, aError);
  }
}

static void EX_Alloc(ExternalCallContext& aCx) {
  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
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

  EX_CreateCFTypeRval(aCx);
}

static void EX_PerformSelector(ExternalCallContext& aCx) {
  EX_CString<2>(aCx);
  EX_CFTypeArg<3>(aCx);
  auto& selector = aCx.mArguments->Arg<2, const char*>();

  // The behavior of performSelector:withObject: varies depending on the
  // selector used, so use a whitelist here.
  if (aCx.mPhase == ExternalCallPhase::SaveInput) {
    if (strcmp(selector, "appearanceNamed:")) {
      aCx.MarkAsFailed();
      return;
    }
  }

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    selector = (const char*) sel_registerName(selector);
  }

  EX_AutoreleaseCFTypeRval(aCx);
}

static void EX_DictionaryWithObjectsAndKeys(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    size_t numArgs = 0;
    if (aCx.mPhase == ExternalCallPhase::SaveInput) {
      // Advance through the arguments until there is a null value. If there
      // are too many arguments for the underlying CallArguments, we will
      // safely crash when we hit their extent.
      for (numArgs = 2;; numArgs += 2) {
        auto& value = aCx.mArguments->Arg<id>(numArgs);
        if (!value) {
          break;
        }
      }
    }
    aCx.ReadOrWriteInputBytes(&numArgs, sizeof(numArgs));

    for (size_t i = 0; i < numArgs; i += 2) {
      auto& value = aCx.mArguments->Arg<id>(i);
      auto& key = aCx.mArguments->Arg<id>(i + 1);
      EX_ObjCInput(aCx, &value);
      EX_ObjCInput(aCx, &key);
    }
  }

  EX_AutoreleaseCFTypeRval(aCx);
}

static void EX_DictionaryWithObjects(ExternalCallContext& aCx) {
  EX_Buffer<2, 4, const void*, false>(aCx);
  EX_Buffer<3, 4, const void*, false>(aCx);

  if (aCx.AccessInput()) {
    auto objects = aCx.mArguments->Arg<2, const void**>();
    auto keys = aCx.mArguments->Arg<3, const void**>();
    auto count = aCx.mArguments->Arg<4, CFIndex>();

    for (CFIndex i = 0; i < count; i++) {
      EX_ObjCInput(aCx, (id*)&objects[i]);
      EX_ObjCInput(aCx, (id*)&keys[i]);
    }
  }

  EX_AutoreleaseCFTypeRval(aCx);
}

static void EX_NSStringGetCharacters(ExternalCallContext& aCx) {
  auto string = aCx.mArguments->Arg<0, CFStringRef>();
  auto& buffer = aCx.mArguments->Arg<2, void*>();

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    size_t len = CFStringGetLength(string);
    buffer = aCx.AllocateBytes(len * sizeof(UniChar));
  }

  if (aCx.AccessOutput()) {
    size_t len = 0;
    if (aCx.mPhase == ExternalCallPhase::SaveOutput) {
      if (IsReplaying()) {
        len = Thread::Current()->mRedirectionValue;
      } else {
        len = CFStringGetLength(string);
      }
    }
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    aCx.ReadOrWriteOutputBytes(buffer, len * sizeof(UniChar));
  }
}

struct ObjCMessageInfo {
  const char* mMessage;
  ExternalCallFn mExternalCall;
  bool mUpdatesObject;
};

// All Objective C messages that can be called in the middleman, and hooks for
// capturing any inputs and outputs other than the object, message, and scalar
// arguments / return values.
static ObjCMessageInfo gObjCExternalCallMessages[] = {
    // Generic
    {"alloc", EX_Alloc},
    {"init", EX_AutoreleaseCFTypeRval},
    {"performSelector:withObject:", EX_PerformSelector},
    {"release", EX_SkipExecuting},
    {"respondsToSelector:", EX_CString<2>},

    // NSAppearance
    {"_drawInRect:context:options:",
     EX_Compose<EX_StackArgumentData<sizeof(CGRect)>, EX_CFTypeArg<2>,
                EX_CFTypeArg<3>>},

    // NSAutoreleasePool
    {"drain", EX_SkipExecuting},

    // NSArray
    {"count"},
    {"objectAtIndex:", EX_Compose<EX_ScalarArg<2>, EX_AutoreleaseCFTypeRval>},

    // NSBezierPath
    {"addClip", EX_NoOp, true},
    {"bezierPathWithRoundedRect:xRadius:yRadius:",
     EX_Compose<EX_StackArgumentData<sizeof(CGRect)>,
                EX_FloatArg<0>, EX_FloatArg<1>, EX_AutoreleaseCFTypeRval>},

    // NSCell
    {"drawFocusRingMaskWithFrame:inView:",
     EX_Compose<EX_CFTypeArg<2>, EX_StackArgumentData<sizeof(CGRect)>>},
    {"drawWithFrame:inView:",
     EX_Compose<EX_CFTypeArg<2>, EX_StackArgumentData<sizeof(CGRect)>>},
    {"initTextCell:", EX_Compose<EX_CFTypeArg<2>, EX_AutoreleaseCFTypeRval>},
    {"initTextCell:pullsDown:",
     EX_Compose<EX_CFTypeArg<2>, EX_ScalarArg<3>, EX_AutoreleaseCFTypeRval>},
    {"setAllowsMixedState:", EX_ScalarArg<2>, true},
    {"setBezeled:", EX_ScalarArg<2>, true},
    {"setBezelStyle:", EX_ScalarArg<2>, true},
    {"setButtonType:", EX_ScalarArg<2>, true},
    {"setControlSize:", EX_ScalarArg<2>, true},
    {"setControlTint:", EX_ScalarArg<2>, true},
    {"setCriticalValue:", EX_FloatArg<0>, true},
    {"setDoubleValue:", EX_FloatArg<0>, true},
    {"setEditable:", EX_ScalarArg<2>, true},
    {"setEnabled:", EX_ScalarArg<2>, true},
    {"setFocusRingType:", EX_ScalarArg<2>, true},
    {"setHighlighted:", EX_ScalarArg<2>, true},
    {"setHighlightsBy:", EX_ScalarArg<2>, true},
    {"setHorizontal:", EX_ScalarArg<2>, true},
    {"setIndeterminate:", EX_ScalarArg<2>, true},
    {"setMax:", EX_FloatArg<0>, true},
    {"setMaxValue:", EX_FloatArg<0>, true},
    {"setMinValue:", EX_FloatArg<0>, true},
    {"setPlaceholderString:", EX_CFTypeArg<2>, true},
    {"setPullsDown:", EX_ScalarArg<2>, true},
    {"setShowsFirstResponder:", EX_ScalarArg<2>, true},
    {"setState:", EX_ScalarArg<2>, true},
    {"setValue:", EX_FloatArg<0>, true},
    {"setWarningValue:", EX_FloatArg<0>, true},
    {"showsFirstResponder"},

    // NSColor
    {"alphaComponent"},
    {"colorWithDeviceRed:green:blue:alpha:",
     EX_Compose<EX_FloatArg<0>, EX_FloatArg<1>, EX_FloatArg<2>,
                EX_StackArgumentData<sizeof(CGFloat)>,
                EX_AutoreleaseCFTypeRval>},
    {"currentControlTint"},
    {"set", EX_NoOp, true},

    // NSDictionary
    {"dictionaryWithObjectsAndKeys:", EX_DictionaryWithObjectsAndKeys},
    {"dictionaryWithObjects:forKeys:count:", EX_DictionaryWithObjects},
    {"mutableCopy", EX_AutoreleaseCFTypeRval},
    {"setObject:forKey:", EX_Compose<EX_CFTypeArg<2>, EX_CFTypeArg<3>>, true},

    // NSFont
    {"boldSystemFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},
    {"controlContentFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},
    {"familyName", EX_AutoreleaseCFTypeRval},
    {"fontDescriptor", EX_AutoreleaseCFTypeRval},
    {"menuBarFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},
    {"pointSize"},
    {"smallSystemFontSize"},
    {"systemFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},
    {"toolTipsFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},
    {"userFontOfSize:",
     EX_Compose<EX_FloatArg<0>, EX_AutoreleaseCFTypeRval>},

    // NSFontManager
    {"availableMembersOfFontFamily:",
     EX_Compose<EX_CFTypeArg<2>, EX_AutoreleaseCFTypeRval>},
    {"sharedFontManager", EX_AutoreleaseCFTypeRval},

    // NSGraphicsContext
    {"currentContext", EX_AutoreleaseCFTypeRval},
    {"graphicsContextWithGraphicsPort:flipped:",
     EX_Compose<EX_CFTypeArg<2>, EX_ScalarArg<3>, EX_AutoreleaseCFTypeRval>},
    {"graphicsPort", EX_AutoreleaseCFTypeRval},
    {"restoreGraphicsState"},
    {"saveGraphicsState"},
    {"setCurrentContext:", EX_CFTypeArg<2>},

    // NSNumber
    {"numberWithBool:", EX_Compose<EX_ScalarArg<2>, EX_AutoreleaseCFTypeRval>},
    {"unsignedIntValue"},

    // NSString
    {"getCharacters:", EX_NSStringGetCharacters},
    {"hasSuffix:", EX_CFTypeArg<2>},
    {"isEqualToString:", EX_CFTypeArg<2>},
    {"length"},
    {"rangeOfString:options:", EX_Compose<EX_CFTypeArg<2>, EX_ScalarArg<3>>},
    {"stringWithCharacters:length:",
     EX_Compose<EX_Buffer<2, 3, UniChar>, EX_AutoreleaseCFTypeRval>},

    // NSWindow
    {"coreUIRenderer", EX_AutoreleaseCFTypeRval},

    // UIFontDescriptor
    {"symbolicTraits"},
};

static void EX_objc_msgSend(ExternalCallContext& aCx) {
  EX_CString<1>(aCx);
  auto& message = aCx.mArguments->Arg<1, const char*>();

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    message = (const char*) sel_registerName(message);
  }

  for (const ObjCMessageInfo& info : gObjCExternalCallMessages) {
    if (!strcmp(info.mMessage, message)) {
      if (info.mUpdatesObject) {
        EX_UpdateCFTypeArg<0>(aCx);
      } else {
        EX_CFTypeArg<0>(aCx);
      }
      if (info.mExternalCall && !aCx.mFailed) {
        info.mExternalCall(aCx);
      }
      if (aCx.mFailed && HasDivergedFromRecording()) {
        PrintSpew("Middleman message failure: %s\n", message);
        if (child::CurrentRepaintCannotFail()) {
          child::ReportFatalError("Middleman message failure: %s\n", message);
        }
      }
      return;
    }
  }

  if (aCx.mPhase == ExternalCallPhase::SaveInput) {
    aCx.MarkAsFailed();
    if (HasDivergedFromRecording()) {
      PrintSpew("Middleman message failure: %s\n", message);
      if (child::CurrentRepaintCannotFail()) {
        child::ReportFatalError("Could not perform middleman message: %s\n",
                                message);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Cocoa redirections
///////////////////////////////////////////////////////////////////////////////

static void EX_CFArrayCreate(ExternalCallContext& aCx) {
  EX_RequireDefaultAllocator<0>(aCx);
  EX_Buffer<1, 2, const void*, false>(aCx);
  EX_RequireFixed<3, FixedInput::kCFTypeArrayCallBacks>(aCx);

  if (aCx.AccessInput()) {
    auto values = aCx.mArguments->Arg<1, const void**>();
    auto numValues = aCx.mArguments->Arg<2, CFIndex>();

    for (CFIndex i = 0; i < numValues; i++) {
      EX_ObjCInput(aCx, (id*)&values[i]);
    }
  }

  EX_CreateCFTypeRval(aCx);
}

static void EX_CFArrayGetValueAtIndex(ExternalCallContext& aCx) {
  EX_CFTypeArg<0>(aCx);
  EX_ScalarArg<1>(aCx);

  auto array = aCx.mArguments->Arg<0, id>();

  // We can't probe the array to see what callbacks it uses, so look at where
  // it came from to see whether its elements should be treated as CFTypes.
  ExternalCall* call = LookupExternalCall(array);
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
    EX_CFTypeRval(aCx);
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

  // Save the length so that EX_CFDataGetBytePtr can recover it when saving
  // output while replaying.
  Thread::Current()->mRedirectionValue = len;
}

static void EX_CFDataGetBytePtr(ExternalCallContext& aCx) {
  EX_CFTypeArg<0>(aCx);

  auto data = aCx.mArguments->Arg<0, CFDataRef>();
  auto& buffer = aCx.mArguments->Rval<void*>();

  if (aCx.AccessOutput()) {
    size_t len = 0;
    if (aCx.mPhase == ExternalCallPhase::SaveOutput) {
      if (IsReplaying()) {
        len = Thread::Current()->mRedirectionValue;
      } else {
        len = CFDataGetLength(data);
      }
    }
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mPhase == ExternalCallPhase::RestoreOutput) {
      buffer = aCx.AllocateBytes(len);
    }
    aCx.ReadOrWriteOutputBytes(buffer, len);
  }
}

static void EX_CFDictionaryCreate(ExternalCallContext& aCx) {
  EX_RequireDefaultAllocator<0>(aCx);
  EX_Buffer<1, 3, const void*, false>(aCx);
  EX_Buffer<2, 3, const void*, false>(aCx);
  EX_RequireFixed<4, FixedInput::kCFTypeDictionaryKeyCallBacks>(aCx);
  EX_RequireFixed<5, FixedInput::kCFTypeDictionaryValueCallBacks>(aCx);

  if (aCx.AccessInput()) {
    auto keys = aCx.mArguments->Arg<1, const void**>();
    auto values = aCx.mArguments->Arg<2, const void**>();
    auto numValues = aCx.mArguments->Arg<3, CFIndex>();

    for (CFIndex i = 0; i < numValues; i++) {
      EX_ObjCInput(aCx, (id*)&keys[i]);
      EX_ObjCInput(aCx, (id*)&values[i]);
    }
  }

  EX_CreateCFTypeRval(aCx);
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

static void EX_CFNumberCreate(ExternalCallContext& aCx) {
  EX_RequireDefaultAllocator<0>(aCx);
  EX_ScalarArg<1>(aCx);

  if (aCx.AccessInput()) {
    auto numberType = aCx.mArguments->Arg<1, CFNumberType>();
    auto& valuePtr = aCx.mArguments->Arg<2, void*>();
    aCx.ReadOrWriteInputBuffer(&valuePtr, CFNumberTypeBytes(numberType));
  }

  EX_CreateCFTypeRval(aCx);
}

static void RR_CFNumberGetValue(Stream& aEvents, CallArguments* aArguments,
                                ErrorType* aError) {
  auto& type = aArguments->Arg<1, CFNumberType>();
  auto& value = aArguments->Arg<2, void*>();

  aEvents.CheckInput(type);
  aEvents.RecordOrReplayBytes(value, CFNumberTypeBytes(type));
}

static void EX_CFNumberGetValue(ExternalCallContext& aCx) {
  EX_CFTypeArg<0>(aCx);
  EX_ScalarArg<1>(aCx);

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

  // When replaying, EX_CGBitmapContextCreateWithData will take care of
  // updating gContextData with the right context pointer.
  if (IsRecording()) {
    gContextData.emplaceBack(rval, data, height * bytesPerRow);
  }
}

static void EX_CGBitmapContextCreateWithData(ExternalCallContext& aCx) {
  EX_ScalarArg<1>(aCx);
  EX_ScalarArg<2>(aCx);
  EX_ScalarArg<3>(aCx);
  EX_ScalarArg<4>(aCx);
  EX_CFTypeArg<5>(aCx);
  EX_ScalarArg<6>(aCx);
  EX_CreateCFTypeRval(aCx);

  auto& data = aCx.mArguments->Arg<0, void*>();
  auto height = aCx.mArguments->Arg<2, size_t>();
  auto bytesPerRow = aCx.mArguments->Arg<4, size_t>();
  auto rval = aCx.mArguments->Rval<CGContextRef>();

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    data = aCx.AllocateBytes(height * bytesPerRow);

    auto& releaseCallback = aCx.mArguments->Arg<7, void*>();
    auto& releaseInfo = aCx.mArguments->Arg<8, void*>();
    releaseCallback = nullptr;
    releaseInfo = nullptr;
  }

  if (aCx.AccessOutput()) {
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
static void EX_FlushCGContext(ExternalCallContext& aCx) {
  auto context = aCx.mArguments->Arg<ContextArgument, CGContextRef>();

  // Update the contents of the target buffer in the middleman process to match
  // the current contents in the replaying process.
  if (aCx.AccessInput()) {
    for (int i = gContextData.length() - 1; i >= 0; i--) {
      if (context == gContextData[i].mContext) {
        // The initial context data is not used to characterize the call,
        // such that two calls with different context data and otherwise
        // identical inputs will use the same output. This avoids performing
        // calls in external processes when Gecko drawing routines will not be
        // using all of the output of the drawn context, but is unsound.
        aCx.ReadOrWriteInputBytes(gContextData[i].mData,
                                  gContextData[i].mDataSize,
                                  /* aExcludeInput */ true);
        return;
      }
    }
    MOZ_CRASH("EX_FlushCGContext");
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
    MOZ_CRASH("EX_FlushCGContext");
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

static void EX_CGDataProviderCreateWithData(ExternalCallContext& aCx) {
  EX_Buffer<1, 2>(aCx);
  EX_CreateCFTypeRval(aCx);

  auto& info = aCx.mArguments->Arg<0, void*>();
  auto& data = aCx.mArguments->Arg<1, const void*>();
  auto& size = aCx.mArguments->Arg<2, size_t>();
  auto& releaseData =
      aCx.mArguments->Arg<3, CGDataProviderReleaseDataCallback>();

  // Make a copy of the data that won't be released the next time middleman
  // calls are reset, in case CoreGraphics decides to hang onto the data
  // provider after that point.
  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    void* newData = malloc(size);
    memcpy(newData, data, size);
    data = newData;
    releaseData = ReleaseDataCallback;
  }

  // Immediately release the data in the replaying process.
  if (aCx.mPhase == ExternalCallPhase::SaveInput && releaseData) {
    releaseData(info, data, size);
  }
}

static size_t CGPathElementPointCount(CGPathElementType aType) {
  switch (aType) {
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

static void CGPathDataCallback(void* aData, const CGPathElement* aElement) {
  InfallibleVector<char>* data = (InfallibleVector<char>*)aData;

  data->append((char)aElement->type);

  for (size_t i = 0; i < CGPathElementPointCount(aElement->type); i++) {
    data->append((char*)&aElement->points[i], sizeof(CGPoint));
  }
}

static void GetCGPathData(CGPathRef aPath, InfallibleVector<char>& aData) {
  CGPathApply(aPath, &aData, CGPathDataCallback);
}

static void CGPathApplyWithData(const InfallibleVector<char>& aData,
                                void* aInfo, CGPathApplierFunction aFunction) {
  size_t offset = 0;
  while (offset < aData.length()) {
    CGPathElement element;
    element.type = (CGPathElementType)aData[offset++];

    CGPoint points[3];
    element.points = points;
    size_t pointCount = CGPathElementPointCount(element.type);
    MOZ_RELEASE_ASSERT(pointCount <= ArrayLength(points));
    for (size_t i = 0; i < pointCount; i++) {
      memcpy(&points[i], &aData[offset], sizeof(CGPoint));
      offset += sizeof(CGPoint);
    }

    aFunction(aInfo, &element);
  }
}

static PreambleResult Preamble_CGPathApply(CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough() || HasDivergedFromRecording()) {
    return PreambleResult::Redirect;
  }

  auto& path = aArguments->Arg<0, CGPathRef>();
  auto& info = aArguments->Arg<1, void*>();
  auto& function = aArguments->Arg<2, CGPathApplierFunction>();

  InfallibleVector<char> pathData;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    GetCGPathData(path, pathData);
  }
  size_t len = RecordReplayValue(pathData.length());
  if (IsReplaying()) {
    pathData.appendN(0, len);
  }
  RecordReplayBytes(pathData.begin(), len);

  CGPathApplyWithData(pathData, info, function);

  Thread* thread = Thread::Current();
  thread->mRedirectionData.clear();
  thread->mRedirectionData.append(pathData.begin(), len);

  return PreambleResult::Veto;
}

static void EX_CGPathApply(ExternalCallContext& aCx) {
  EX_CFTypeArg<0>(aCx);
  EX_SkipExecuting(aCx);

  if (aCx.AccessOutput()) {
    InfallibleVector<char> pathData;
    if (aCx.mPhase == ExternalCallPhase::SaveOutput) {
      if (IsReplaying()) {
        Thread* thread = Thread::Current();
        pathData.append(thread->mRedirectionData.begin(),
                        thread->mRedirectionData.length());
      } else {
        auto path = aCx.mArguments->Arg<0, CGPathRef>();
        GetCGPathData(path, pathData);
      }
    }
    size_t len = pathData.length();
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mPhase == ExternalCallPhase::RestoreOutput) {
      pathData.appendN(0, len);
    }
    aCx.ReadOrWriteOutputBytes(pathData.begin(), len);

    if (aCx.mPhase == ExternalCallPhase::RestoreOutput) {
      auto& data = aCx.mArguments->Arg<1, void*>();
      auto& function = aCx.mArguments->Arg<2, CGPathApplierFunction>();
      CGPathApplyWithData(pathData, data, function);
    }
  }
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

  Thread::Current()->mRedirectionValue = count;
}

template <typename ElemType, void (*GetElemsFn)(CTRunRef, CFRange, ElemType*)>
static void EX_CTRunGetElements(ExternalCallContext& aCx) {
  EX_CFTypeArg<0>(aCx);

  if (aCx.AccessOutput()) {
    auto run = aCx.mArguments->Arg<0, CTRunRef>();
    auto& rval = aCx.mArguments->Rval<ElemType*>();

    size_t count = 0;
    if (aCx.mPhase == ExternalCallPhase::SaveOutput) {
      if (IsReplaying()) {
        count = Thread::Current()->mRedirectionValue;
      } else {
        count = CTRunGetGlyphCount(run);
        if (!rval) {
          rval = (ElemType*)aCx.AllocateBytes(count * sizeof(ElemType));
          GetElemsFn(run, CFRangeMake(0, 0), rval);
        }
      }
    }
    aCx.ReadOrWriteOutputBytes(&count, sizeof(count));
    if (aCx.mPhase == ExternalCallPhase::RestoreOutput) {
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
  AutoEnsurePassThroughThreadEvents pt;
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
  ExternalCallFn mExternalCall;
  PreambleFn mExternalPreamble;
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
    {"mmap", nullptr, Preamble_mmap},
    {"read", RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>, nullptr,
     nullptr, Preamble_SetError<EIO>},
    {"__read_nocancel",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>},
    {"pread", RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>},
    {"write", RR_SaveRvalHadErrorNegative, nullptr, nullptr,
     MiddlemanPreamble_write},
    {"__write_nocancel", RR_SaveRvalHadErrorNegative},
    {"open", RR_open},
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
    {"close", RR_close, nullptr, nullptr, Preamble_Veto<0>},
    {"__close_nocancel", RR_SaveRvalHadErrorNegative},
    {"mkdir", RR_SaveRvalHadErrorNegative},
    {"dup", RR_SaveRvalHadErrorNegative},
    {"access", RR_SaveRvalHadErrorNegative, nullptr, nullptr,
     Preamble_SetError<EACCES>},
    {"lseek", RR_SaveRvalHadErrorNegative},
    {"select$DARWIN_EXTSN",
     RR_SaveRvalHadErrorNegative<RR_Compose<RR_OutParam<1, fd_set>,
                                            RR_OutParam<2, fd_set>,
                                            RR_OutParam<3, fd_set>>>,
     nullptr, nullptr, Preamble_WaitForever},
    {"socketpair",
     RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<3, 2 * sizeof(int)>>},
    {"fileport_makeport", RR_SaveRvalHadErrorNegative<RR_OutParam<1, size_t>>},
    {"getsockopt", RR_SaveRvalHadErrorNegative<RR_getsockopt>},
    {"gettimeofday",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_OutParam<0, struct timeval>,
         RR_OutParam<1, struct timezone>>>,
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
    {"fstat$INODE64", RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct stat>>,
     nullptr, nullptr, Preamble_SetError},
    {"lstat$INODE64", RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct stat>>,
     nullptr, nullptr, Preamble_SetError},
    {"stat$INODE64", RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct stat>>,
     nullptr, nullptr, Preamble_SetError},
    {"statfs$INODE64",
     RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct statfs>>,
     nullptr, nullptr, Preamble_SetError},
    {"fstatfs$INODE64",
     RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct statfs>>,
     nullptr, nullptr, Preamble_SetError},
    {"readlink", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<1, 2>>},
    {"__getdirentries64",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_WriteBuffer<1, 2>, RR_OutParam<3, size_t>>>},
    {"getdirentriesattr",
     RR_SaveRvalHadErrorNegative<RR_Compose<
         RR_WriteBufferFixedSize<1, sizeof(struct attrlist)>,
         RR_WriteBuffer<2, 3>,
         RR_OutParam<4, size_t>,
         RR_OutParam<5, size_t>,
         RR_OutParam<6, size_t>>>},
    {"getrusage", RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct rusage>>,
     nullptr, nullptr, Preamble_SetError},
    {"getrlimit", RR_SaveRvalHadErrorNegative<RR_OutParam<1, struct rlimit>>},
    {"setrlimit", RR_SaveRvalHadErrorNegative},
    {"sigprocmask", RR_SaveRvalHadErrorNegative<RR_OutParam<2, sigset_t>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"sigaltstack", RR_SaveRvalHadErrorNegative<RR_OutParam<2, stack_t>>},
    {"sigaction",
     RR_SaveRvalHadErrorNegative<RR_OutParam<2, struct sigaction>>},
    {"signal", RR_ScalarRval},
    {"__pthread_sigmask",
     RR_SaveRvalHadErrorNegative<RR_OutParam<2, sigset_t>>},
    {"__fsgetpath", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>},
    {"sysconf", RR_ScalarRval},
    {"__sysctl", RR_SaveRvalHadErrorNegative<RR_sysctl<2>>},
    {"sysctl", RR_SaveRvalHadErrorNegative<RR_sysctl<2>>},
    {"sysctlbyname", RR_SaveRvalHadErrorNegative<RR_sysctl<1>>,
     Preamble_sysctlbyname, nullptr, Preamble_SetError},
    {"__mac_syscall", RR_SaveRvalHadErrorNegative},
    {"syscall", RR_SaveRvalHadErrorNegative},
    {"getaudit_addr",
     RR_SaveRvalHadErrorNegative<RR_OutParam<0, auditinfo_addr_t>>},
    {"umask", RR_ScalarRval},
    {"__select",
     RR_SaveRvalHadErrorNegative<
         RR_Compose<RR_OutParam<1, fd_set>,
                    RR_OutParam<2, fd_set>,
                    RR_OutParam<3, fd_set>,
                    RR_OutParam<4, timeval>>>,
     nullptr, nullptr, Preamble_WaitForever},
    {"__process_policy", RR_SaveRvalHadErrorNegative},
    {"__kdebug_trace", RR_SaveRvalHadErrorNegative},
    {"guarded_kqueue_np",
     RR_SaveRvalHadErrorNegative<RR_OutParam<0, size_t>>},
    {"csops", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 3>>},
    {"__getlogin", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>},
    {"__workq_kernreturn", nullptr, Preamble___workq_kernreturn},
    {"start_wqthread", nullptr, Preamble_start_wqthread},
    {"isatty", RR_SaveRvalHadErrorZero},

    /////////////////////////////////////////////////////////////////////////////
    // PThreads interfaces
    /////////////////////////////////////////////////////////////////////////////

    {"pthread_atfork", nullptr, Preamble_Veto<0>},
    {"pthread_cond_wait", nullptr, Preamble_pthread_cond_wait},
    {"pthread_cond_timedwait", nullptr, Preamble_pthread_cond_timedwait},
    {"pthread_cond_timedwait_relative_np", nullptr,
     Preamble_pthread_cond_timedwait_relative_np},
    {"pthread_create", nullptr, Preamble_pthread_create},
    {"pthread_get_stacksize_np", nullptr, Preamble_PassThrough},
    {"pthread_get_stackaddr_np", nullptr, Preamble_PassThrough},
    {"pthread_join", nullptr, Preamble_pthread_join},
    {"pthread_mutex_init", nullptr, Preamble_pthread_mutex_init},
    {"pthread_mutex_destroy", nullptr, Preamble_pthread_mutex_destroy},
    {"pthread_mutex_lock", nullptr, Preamble_pthread_mutex_lock},
    {"pthread_mutex_trylock", nullptr, Preamble_pthread_mutex_trylock},
    {"pthread_mutex_unlock", nullptr, Preamble_pthread_mutex_unlock},
    {"pthread_getspecific", nullptr, Preamble_pthread_getspecific},
    {"pthread_setspecific", nullptr, Preamble_pthread_setspecific},
    {"pthread_self", nullptr, Preamble_pthread_self},
    {"_tlv_bootstrap", nullptr, Preamble__tlv_bootstrap},

    /////////////////////////////////////////////////////////////////////////////
    // C library functions
    /////////////////////////////////////////////////////////////////////////////

    {"closedir"},
    {"dlclose", nullptr, Preamble_Veto<0>},
    {"dlopen", nullptr, Preamble_dlopen},
    {"dlsym", nullptr, Preamble_dlsym},
    {"fclose", RR_SaveRvalHadErrorNegative},
    {"fprintf", RR_SaveRvalHadErrorNegative},
    {"fopen", RR_SaveRvalHadErrorZero},
    {"fread", RR_Compose<RR_ScalarRval, RR_fread>},
    {"fseek", RR_SaveRvalHadErrorNegative},
    {"ftell", RR_SaveRvalHadErrorNegative},
    {"fwrite", RR_ScalarRval},
    {"getenv", RR_CStringRval, Preamble_getenv, nullptr, Preamble_Veto<0>},
    {"localtime_r",
     RR_SaveRvalHadErrorZero<RR_Compose<RR_OutParam<1, struct tm>,
                                        RR_RvalIsArgument<1>>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"gmtime_r",
     RR_SaveRvalHadErrorZero<RR_Compose<RR_OutParam<1, struct tm>,
                                        RR_RvalIsArgument<1>>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"localtime", nullptr, Preamble_localtime, nullptr, Preamble_PassThrough},
    {"gmtime", nullptr, Preamble_gmtime, nullptr, Preamble_PassThrough},
    {"mktime",
     RR_Compose<RR_ScalarRval, RR_OutParam<0, struct tm>>},
    {"setlocale", RR_CStringRval},
    {"strftime", RR_Compose<RR_ScalarRval, RR_WriteBufferViaRval<0, 1, 1>>},
    {"arc4random", RR_ScalarRval, nullptr, nullptr, Preamble_PassThrough},
    {"arc4random_buf", RR_WriteBuffer<0, 1>},
    {"bootstrap_look_up", RR_OutParam<2, mach_port_t>},
    {"clock_gettime", RR_Compose<RR_ScalarRval, RR_OutParam<1, timespec>>},
    {"clock_get_time",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, mach_timespec_t>>},
    {"host_get_clock_service",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, clock_serv_t>>},
    {"host_info", RR_host_info_or_statistics, nullptr, nullptr,
     Preamble_Veto<KERN_FAILURE>},
    {"host_statistics", RR_host_info_or_statistics},
    {"mach_absolute_time", RR_ScalarRval, nullptr, nullptr,
     Preamble_PassThrough},
    {"mach_host_self", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"mach_msg", RR_Compose<RR_ScalarRval, RR_WriteBuffer<0, 3>>, nullptr,
     nullptr, Preamble_WaitForever},
    {"mach_port_allocate",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, mach_port_t>>},
    {"mach_port_deallocate", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"mach_port_insert_right", RR_ScalarRval},
    {"mach_thread_self", nullptr, Preamble_mach_thread_self},
    {"mach_timebase_info",
     RR_Compose<RR_ScalarRval, RR_OutParam<0, mach_timebase_info_data_t>>},
    {"mach_vm_region", nullptr, Preamble_VetoIfNotPassedThrough<KERN_FAILURE>},
    {"opendir$INODE64", RR_ScalarRval},
    {"rand", RR_ScalarRval},
    {"readdir$INODE64", RR_readdir},
    {"realpath",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_CStringRval, RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>},
    {"realpath$DARWIN_EXTSN",
     RR_SaveRvalHadErrorZero<RR_Compose<
         RR_CStringRval, RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>},
    {"sandbox_init", RR_ScalarRval},
    {"sandbox_init_with_parameters", RR_ScalarRval},
    {"srand", nullptr, nullptr, nullptr, Preamble_PassThrough},
    {"task_threads", RR_task_threads},
    {"task_get_special_port",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, mach_port_t>>},
    {"task_set_special_port", RR_ScalarRval},
    {"task_swap_exception_ports", RR_ScalarRval}, // Ignore out parameters
    {"time", RR_Compose<RR_ScalarRval, RR_OutParam<0, time_t>>,
     nullptr, nullptr, Preamble_PassThrough},
    {"uname", RR_SaveRvalHadErrorNegative<RR_OutParam<0, utsname>>,
     nullptr, nullptr, Preamble_SetError},
    {"vm_allocate", nullptr, Preamble_VetoIfNotPassedThrough<KERN_FAILURE>},
    {"vm_copy", nullptr, Preamble_PassThrough},
    {"tzset"},
    {"_dyld_register_func_for_add_image"},
    {"_dyld_register_func_for_remove_image"},
    {"setjmp", nullptr, Preamble_VetoIfNotPassedThrough<0>},
    {"longjmp", nullptr, Preamble_NYI},

    /////////////////////////////////////////////////////////////////////////////
    // C++ library functions
    /////////////////////////////////////////////////////////////////////////////

    {"_ZNSt3__16chrono12system_clock3nowEv", RR_ScalarRval},

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
    {"objc_msgSend", RR_objc_msgSend, Preamble_objc_msgSend, EX_objc_msgSend},

    /////////////////////////////////////////////////////////////////////////////
    // Cocoa and CoreFoundation library functions
    /////////////////////////////////////////////////////////////////////////////

    {"AcquireFirstMatchingEventInQueue", RR_ScalarRval},
    {"CFArrayAppendValue"},
    {"CFArrayCreate", RR_ScalarRval, nullptr, EX_CFArrayCreate},
    {"CFArrayCreateMutable", RR_ScalarRval},
    {"CFArrayGetCount", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CFArrayGetValueAtIndex", RR_ScalarRval, nullptr,
     EX_CFArrayGetValueAtIndex},
    {"CFArrayRemoveValueAtIndex"},
    {"CFAttributedStringCreate", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>,
                EX_CFTypeArg<1>, EX_CFTypeArg<2>, EX_CreateCFTypeRval>},
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
    {"CFDataAppendBytes"},
    {"CFDataCreateMutable", RR_ScalarRval},
    {"CFDataGetBytePtr", RR_CFDataGetBytePtr, nullptr, EX_CFDataGetBytePtr},
    {"CFDataGetLength", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CFDateFormatterCreate", RR_ScalarRval},
    {"CFDateFormatterGetFormat", RR_ScalarRval},
    {"CFDictionaryAddValue", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_CFTypeArg<1>, EX_CFTypeArg<2>>},
    {"CFDictionaryCreate", RR_ScalarRval, nullptr, EX_CFDictionaryCreate},
    {"CFDictionaryCreateMutable", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>,
                EX_ScalarArg<1>,
                EX_RequireFixed<2,
                    FixedInput::kCFTypeDictionaryKeyCallBacks>,
                EX_RequireFixed<3,
                    FixedInput::kCFTypeDictionaryValueCallBacks>,
                EX_CreateCFTypeRval>},
    {"CFDictionaryCreateMutableCopy", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>,
                EX_ScalarArg<1>, EX_CFTypeArg<2>, EX_CreateCFTypeRval>},
    {"CFDictionaryGetTypeID", RR_ScalarRval, nullptr, EX_NoOp},
    {"CFDictionaryGetValue", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CFTypeRval>},
    {"CFDictionaryGetValueIfPresent",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, const void*>>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CFTypeOutputArg<2>>},
    {"CFDictionaryReplaceValue", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_CFTypeArg<1>, EX_CFTypeArg<2>>},
    {"CFEqual", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>>},
    {"CFGetTypeID", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CFLocaleCopyCurrent", RR_ScalarRval},
    {"CFLocaleCopyPreferredLanguages", RR_ScalarRval},
    {"CFLocaleCreate", RR_ScalarRval},
    {"CFLocaleGetIdentifier", RR_ScalarRval},
    {"CFNotificationCenterAddObserver", nullptr,
     Preamble_CFNotificationCenterAddObserver},
    {"CFNotificationCenterGetLocalCenter", RR_ScalarRval},
    {"CFNotificationCenterRemoveObserver"},
    {"CFNumberCreate", RR_ScalarRval, nullptr, EX_CFNumberCreate},
    {"CFNumberGetTypeID", RR_ScalarRval, nullptr, EX_NoOp},
    {"CFNumberGetValue", RR_Compose<RR_ScalarRval, RR_CFNumberGetValue>,
     nullptr, EX_CFNumberGetValue},
    {"CFNumberIsFloatType", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CFPreferencesAppValueIsForced", RR_ScalarRval},
    {"CFPreferencesCopyAppValue", RR_ScalarRval},
    {"CFPreferencesCopyValue", RR_ScalarRval},
    {"CFPropertyListCreateFromXMLData", RR_ScalarRval},
    {"CFPropertyListCreateWithData", RR_ScalarRval},
    {"CFPropertyListCreateWithStream", RR_ScalarRval},
    {"CFReadStreamClose"},
    {"CFReadStreamCreateWithFile", RR_ScalarRval},
    {"CFReadStreamOpen", RR_ScalarRval},
    {"CFReadStreamRead", RR_Compose<RR_ScalarRval, RR_WriteBuffer<1, 2>>},

    // Don't handle release/retain calls explicitly in the middleman:
    // all resources will be cleaned up when its calls are reset.
    {"CFRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CGContextRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CFRetain", RR_ScalarRval, nullptr, nullptr, MiddlemanPreamble_CFRetain},
    {"CGContextRetain", RR_ScalarRval, nullptr, nullptr,
     MiddlemanPreamble_CFRetain},
    {"CGFontRetain", RR_ScalarRval, nullptr, nullptr,
     MiddlemanPreamble_CFRetain},

    {"CFRunLoopAddSource"},
    {"CFRunLoopGetCurrent", RR_ScalarRval},
    {"CFRunLoopRemoveSource"},
    {"CFRunLoopSourceCreate", RR_ScalarRval, Preamble_CFRunLoopSourceCreate},
    {"CFRunLoopSourceInvalidate"},
    {"CFRunLoopSourceSignal"},
    {"CFRunLoopWakeUp"},
    {"CFStringAppendCharacters"},
    {"CFStringCompare", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>>},
    {"CFStringCreateArrayBySeparatingStrings", RR_ScalarRval},
    {"CFStringCreateMutable", RR_ScalarRval},
    {"CFStringCreateWithBytes", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>,
                EX_Buffer<1, 2>, EX_ScalarArg<3>, EX_ScalarArg<4>,
                EX_CreateCFTypeRval>},
    {"CFStringCreateWithBytesNoCopy", RR_ScalarRval},
    {"CFStringCreateWithCharactersNoCopy", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>,
                EX_Buffer<1, 2, UniChar>,
                EX_RequireFixed<3, FixedInput::kCFAllocatorNull>,
                EX_CreateCFTypeRval>},
    {"CFStringCreateWithCString", RR_ScalarRval},
    {"CFStringCreateWithFormat", RR_ScalarRval},
    {"CFStringGetBytes",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     RR_Compose<RR_ScalarRval, RR_WriteOptionalBuffer<6, 7>,
                RR_OutParam<8, CFIndex>>},
    {"CFStringGetCharacters",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     // We also need to specify the argument register with the range's length
     // here.
     RR_WriteBuffer<3, 2, UniChar>, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_ScalarArg<1>,
                EX_WriteBuffer<3, 2, UniChar>>},
    {"CFStringGetCString", RR_Compose<RR_ScalarRval, RR_WriteBuffer<1, 2>>},
    {"CFStringGetCStringPtr", nullptr, Preamble_VetoIfNotPassedThrough<0>},
    {"CFStringGetIntValue", RR_ScalarRval},
    {"CFStringGetLength", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CFStringGetMaximumSizeForEncoding", RR_ScalarRval},
    {"CFStringHasPrefix", RR_ScalarRval},
    {"CFStringGetTypeID", RR_ScalarRval, nullptr, EX_NoOp},
    {"CFStringTokenizerAdvanceToNextToken", RR_ScalarRval},
    {"CFStringTokenizerCreate", RR_ScalarRval},
    {"CFStringTokenizerGetCurrentTokenRange", RR_ComplexScalarRval},
    {"CFURLCreateFromFileSystemRepresentation", RR_ScalarRval},
    {"CFURLCreateFromFSRef", RR_ScalarRval},
    {"CFURLCreateWithFileSystemPath", RR_ScalarRval},
    {"CFURLCreateWithString", RR_ScalarRval},
    {"CFURLGetFileSystemRepresentation",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3>>},
    {"CFURLGetFSRef", RR_Compose<RR_ScalarRval, RR_OutParam<1, FSRef>>},
    {"CFUUIDCreate", RR_ScalarRval, nullptr,
     EX_Compose<EX_RequireDefaultAllocator<0>, EX_CreateCFTypeRval>},
    {"CFUUIDCreateString", RR_ScalarRval},
    {"CFUUIDGetUUIDBytes", RR_ComplexScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGAffineTransformConcat",
     RR_OversizeRval<sizeof(CGAffineTransform)>, nullptr,
     EX_Compose<EX_StackArgumentData<2 * sizeof(CGAffineTransform)>,
                EX_OversizeRval<CGAffineTransform>>},
    {"CGAffineTransformInvert",
     RR_OversizeRval<sizeof(CGAffineTransform)>, nullptr,
     EX_Compose<EX_StackArgumentData<sizeof(CGAffineTransform)>,
                EX_OversizeRval<CGAffineTransform>>},
    {"CGAffineTransformMakeScale",
     RR_OversizeRval<sizeof(CGAffineTransform)>, nullptr,
     EX_Compose<EX_FloatArg<0>,
                EX_FloatArg<1>,
                EX_OversizeRval<CGAffineTransform>>},
    {"CGBitmapContextCreateImage", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CGBitmapContextCreate",
     RR_Compose<RR_ScalarRval, RR_CGBitmapContextCreateWithData>, nullptr,
     EX_CGBitmapContextCreateWithData},
    {"CGBitmapContextCreateWithData",
     RR_Compose<RR_ScalarRval, RR_CGBitmapContextCreateWithData>, nullptr,
     EX_CGBitmapContextCreateWithData},
    {"CGBitmapContextGetBytesPerRow", RR_ScalarRval},
    {"CGBitmapContextGetHeight", RR_ScalarRval},
    {"CGBitmapContextGetWidth", RR_ScalarRval},
    {"CGColorRelease", RR_ScalarRval},
    {"CGColorSpaceCopyICCProfile", RR_ScalarRval},
    {"CGColorSpaceCreateDeviceGray", RR_ScalarRval, nullptr,
     EX_CreateCFTypeRval},
    {"CGColorSpaceCreateDeviceRGB", RR_ScalarRval, nullptr,
     EX_CreateCFTypeRval},
    {"CGColorSpaceCreatePattern", RR_ScalarRval},
    {"CGColorSpaceRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CGContextAddPath", nullptr, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>>},
    {"CGContextBeginTransparencyLayerWithRect", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_StackArgumentData<sizeof(CGRect)>,
                EX_CFTypeArg<1>>},
    {"CGContextClipToRect", nullptr, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_StackArgumentData<sizeof(CGRect)>>},
    {"CGContextClipToRects", nullptr, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 2, CGRect>>},
    {"CGContextConcatCTM", nullptr, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextDrawImage", RR_FlushCGContext<0>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_StackArgumentData<sizeof(CGRect)>,
                EX_CFTypeArg<1>, EX_FlushCGContext<0>>},
    {"CGContextDrawLinearGradient", RR_FlushCGContext<0>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>,
                EX_StackArgumentData<2 * sizeof(CGPoint)>,
                EX_ScalarArg<2>,
                EX_FlushCGContext<0>>},
    {"CGContextEndTransparencyLayer", nullptr, nullptr, EX_UpdateCFTypeArg<0>},
    {"CGContextFillPath", RR_FlushCGContext<0>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_FlushCGContext<0>>},
    {"CGContextFillRect", RR_FlushCGContext<0>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_StackArgumentData<sizeof(CGRect)>,
                EX_FlushCGContext<0>>},
    {"CGContextGetClipBoundingBox", RR_OversizeRval<sizeof(CGRect)>},
    {"CGContextGetCTM", RR_OversizeRval<sizeof(CGAffineTransform)>},
    {"CGContextGetType", RR_ScalarRval},
    {"CGContextGetUserSpaceToDeviceSpaceTransform",
     RR_OversizeRval<sizeof(CGAffineTransform)>, nullptr,
     EX_Compose<EX_CFTypeArg<1>, EX_OversizeRval<CGAffineTransform>>},
    {"CGContextRestoreGState", nullptr, Preamble_CGContextRestoreGState,
     EX_UpdateCFTypeArg<0>},
    {"CGContextRotateCTM", nullptr, nullptr,
     EX_Compose<EX_FloatArg<0>, EX_UpdateCFTypeArg<0>>},
    {"CGContextSaveGState", nullptr, nullptr, EX_UpdateCFTypeArg<0>},
    {"CGContextSetAllowsFontSubpixelPositioning", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetAllowsFontSubpixelQuantization", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetAlpha", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_FloatArg<1>>},
    {"CGContextSetBaseCTM", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>,
                EX_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextSetCTM", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>,
                EX_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextSetGrayFillColor", nullptr, nullptr,
     EX_Compose<EX_FloatArg<0>, EX_FloatArg<1>, EX_UpdateCFTypeArg<0>>},
    {"CGContextSetRGBFillColor", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>,
                EX_FloatArg<0>, EX_FloatArg<1>, EX_FloatArg<2>,
                EX_StackArgumentData<sizeof(CGFloat)>>},
    {"CGContextSetRGBStrokeColor", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>,
                EX_FloatArg<0>, EX_FloatArg<1>, EX_FloatArg<2>,
                EX_StackArgumentData<sizeof(CGFloat)>>},
    {"CGContextSetShouldAntialias", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetShouldSmoothFonts", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetShouldSubpixelPositionFonts", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetShouldSubpixelQuantizeFonts", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetTextDrawingMode", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_ScalarArg<1>>},
    {"CGContextSetTextMatrix", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>,
                EX_StackArgumentData<sizeof(CGAffineTransform)>>},
    {"CGContextScaleCTM", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_FloatArg<0>, EX_FloatArg<1>>},
    {"CGContextStrokeLineSegments", RR_FlushCGContext<0>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 2, CGPoint>,
                EX_FlushCGContext<0>>},
    {"CGContextTranslateCTM", nullptr, nullptr,
     EX_Compose<EX_UpdateCFTypeArg<0>, EX_FloatArg<0>, EX_FloatArg<1>>},
    {"CGDataProviderCreateWithData",
     RR_Compose<RR_ScalarRval, RR_CGDataProviderCreateWithData>, nullptr,
     EX_CGDataProviderCreateWithData},
    {"CGDataProviderRelease", nullptr, nullptr, nullptr, Preamble_Veto<0>},
    {"CGDisplayCopyColorSpace", RR_ScalarRval},
    {"CGDisplayIOServicePort", RR_ScalarRval},
    {"CGEventSourceCounterForEventType", RR_ScalarRval},
    {"CGFontCopyTableForTag", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_ScalarArg<1>, EX_CreateCFTypeRval>},
    {"CGFontCopyTableTags", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CGFontCopyVariations", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CGFontCreateCopyWithVariations", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CreateCFTypeRval>},
    {"CGFontCreateWithDataProvider", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CGFontCreateWithFontName", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CGFontCreateWithPlatformFont", RR_ScalarRval},
    {"CGFontGetAscent", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGFontGetCapHeight", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGFontGetDescent", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGFontGetFontBBox", RR_OversizeRval<sizeof(CGRect)>, nullptr,
     EX_Compose<EX_CFTypeArg<1>, EX_OversizeRval<CGRect>>},
    {"CGFontGetGlyphAdvances",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, int>>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 2, CGGlyph>,
                EX_WriteBuffer<3, 2, int>>},
    {"CGFontGetGlyphBBoxes",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, CGRect>>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 2, CGGlyph>,
                EX_WriteBuffer<3, 2, CGRect>>},
    {"CGFontGetGlyphPath", RR_ScalarRval},
    {"CGFontGetLeading", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGFontGetUnitsPerEm", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGFontGetXHeight", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CGGradientCreateWithColorComponents", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 3, CGFloat>,
                EX_Buffer<2, 3, CGFloat>, EX_CreateCFTypeRval>},
    {"CGImageGetHeight", RR_ScalarRval},
    {"CGImageGetWidth", RR_ScalarRval},
    {"CGImageRelease", RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>},
    {"CGMainDisplayID", RR_ScalarRval},
    {"CGPathAddPath"},
    {"CGPathApply", nullptr, Preamble_CGPathApply, EX_CGPathApply},
    {"CGPathContainsPoint", RR_ScalarRval},
    {"CGPathCreateMutable", RR_ScalarRval},
    {"CGPathCreateWithRoundedRect", RR_ScalarRval, nullptr,
     EX_Compose<EX_StackArgumentData<sizeof(CGRect)>,
                EX_FloatArg<0>, EX_FloatArg<1>,
                EX_InParam<0, CGAffineTransform>,
                EX_CreateCFTypeRval>},
    {"CGPathGetBoundingBox", RR_OversizeRval<sizeof(CGRect)>},
    {"CGPathGetCurrentPoint", RR_ComplexFloatRval},
    {"CGPathIsEmpty", RR_ScalarRval},
    {"CGRectApplyAffineTransform", RR_OversizeRval<sizeof(CGRect)>, nullptr,
     EX_Compose<EX_StackArgumentData<sizeof(CGRect) +
                                     sizeof(CGAffineTransform)>,
                EX_OversizeRval<CGRect>>},
    {"CGSSetDebugOptions", RR_ScalarRval},
    {"CGSShutdownServerConnections"},
    {"CTFontCopyFamilyName", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTFontCopyFeatures", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTFontCopyFontDescriptor", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTFontCopyGraphicsFont", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CreateCFTypeRval>},
    {"CTFontCopyTable", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_ScalarArg<1>, EX_ScalarArg<2>,
                EX_CreateCFTypeRval>},
    {"CTFontCopyVariationAxes", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTFontCreateForString", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>,
                EX_ScalarArg<2>, EX_ScalarArg<3>, EX_CreateCFTypeRval>},
    {"CTFontCreatePathForGlyph", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_ScalarArg<1>,
                EX_InParam<2, CGAffineTransform>,
                EX_CreateCFTypeRval>},
    {"CTFontCreateWithFontDescriptor", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_FloatArg<0>,
                EX_InParam<1, CGAffineTransform>,
                EX_CreateCFTypeRval>},
    {"CTFontCreateWithGraphicsFont", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_FloatArg<0>,
                EX_InParam<1, CGAffineTransform>,
                EX_CFTypeArg<2>,
                EX_CreateCFTypeRval>},
    {"CTFontCreateWithName", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>,
                EX_FloatArg<0>,
                EX_InParam<1, CGAffineTransform>,
                EX_CreateCFTypeRval>},
    {"CTFontDescriptorCopyAttribute", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CreateCFTypeRval>},
    {"CTFontDescriptorCreateCopyWithAttributes", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CreateCFTypeRval>},
    {"CTFontDescriptorCreateMatchingFontDescriptors", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CreateCFTypeRval>},
    {"CTFontDescriptorCreateWithAttributes", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTFontDrawGlyphs", RR_FlushCGContext<4>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<4>, EX_Buffer<1, 3, CGGlyph>,
                EX_Buffer<2, 3, CGPoint>, EX_FlushCGContext<4>>},
    {"CTFontGetAdvancesForGlyphs",
     RR_Compose<RR_FloatRval, RR_WriteOptionalBuffer<3, 4, CGSize>>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_ScalarArg<1>,
                EX_Buffer<2, 4, CGGlyph>,
                EX_WriteBuffer<3, 4, CGSize>>},
    {"CTFontGetAscent", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetBoundingBox", RR_OversizeRval<sizeof(CGRect)>, nullptr,
     EX_Compose<EX_CFTypeArg<1>, EX_OversizeRval<CGRect>>},
    {"CTFontGetBoundingRectsForGlyphs",
     // Argument indexes here are off by one due to the oversize rval.
     RR_Compose<RR_OversizeRval<sizeof(CGRect)>,
                RR_WriteOptionalBuffer<4, 5, CGRect>>,
     nullptr,
     EX_Compose<EX_CFTypeArg<1>, EX_ScalarArg<2>,
                EX_Buffer<3, 5, CGGlyph>,
                EX_OversizeRval<CGRect>, EX_WriteBuffer<4, 5, CGRect>>},
    {"CTFontGetCapHeight", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetDescent", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetGlyphCount", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetGlyphsForCharacters",
     RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3, CGGlyph>>, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_Buffer<1, 3, UniChar>,
                EX_WriteBuffer<2, 3, CGGlyph>>},
    {"CTFontGetLeading", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetSize", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetSymbolicTraits", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetUnderlinePosition", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetUnderlineThickness", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetUnitsPerEm", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontGetXHeight", RR_FloatRval, nullptr, EX_CFTypeArg<0>},
    {"CTFontManagerCopyAvailableFontFamilyNames", RR_ScalarRval},
    {"CTFontManagerRegisterFontsForURLs", RR_ScalarRval},
    {"CTFontManagerSetAutoActivationSetting"},
    {"CTLineCreateWithAttributedString", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CreateCFTypeRval>},
    {"CTLineGetGlyphRuns", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeRval>},
    {"CTRunGetAttributes", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeRval>},
    {"CTRunGetGlyphCount", RR_ScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CTRunGetGlyphsPtr", RR_CTRunGetElements<CGGlyph, CTRunGetGlyphs>, nullptr,
     EX_CTRunGetElements<CGGlyph, CTRunGetGlyphs>},
    {"CTRunGetPositionsPtr", RR_CTRunGetElements<CGPoint, CTRunGetPositions>,
     nullptr, EX_CTRunGetElements<CGPoint, CTRunGetPositions>},
    {"CTRunGetStringIndicesPtr",
     RR_CTRunGetElements<CFIndex, CTRunGetStringIndices>, nullptr,
     EX_CTRunGetElements<CFIndex, CTRunGetStringIndices>},
    {"CTRunGetStringRange", RR_ComplexScalarRval, nullptr, EX_CFTypeArg<0>},
    {"CTRunGetTypographicBounds",
     // Argument indexes are off by one here as the CFRange argument uses two
     // slots.
     RR_Compose<RR_FloatRval,
                RR_OutParam<3, CGFloat>,
                RR_OutParam<4, CGFloat>,
                RR_OutParam<5, CGFloat>>,
     nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_ScalarArg<1>, EX_ScalarArg<2>,
                EX_OutParam<3, CGFloat>, EX_OutParam<4, CGFloat>,
                EX_OutParam<5, CGFloat>>},
    {"CUIDraw", nullptr, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeArg<1>, EX_CFTypeArg<2>,
                EX_StackArgumentData<sizeof(CGRect)>>},
    {"FSCompareFSRefs", RR_ScalarRval},
    {"FSGetVolumeInfo",
     RR_Compose<RR_ScalarRval,
                RR_OutParam<5, HFSUniStr255>,
                RR_OutParam<6, FSRef>>},
    {"FSFindFolder", RR_Compose<RR_ScalarRval, RR_OutParam<3, FSRef>>},
    {"Gestalt", RR_Compose<RR_ScalarRval, RR_OutParam<1, SInt32>>},
    {"GetEventClass", RR_ScalarRval},
    {"GetCurrentEventQueue", RR_ScalarRval},
    {"GetCurrentProcess",
     RR_Compose<RR_ScalarRval, RR_OutParam<0, ProcessSerialNumber>>},
    {"GetEventAttributes", RR_ScalarRval},
    {"GetEventDispatcherTarget", RR_ScalarRval},
    {"GetEventKind", RR_ScalarRval},
    {"GetThemeMetric", RR_Compose<RR_ScalarRval, RR_OutParam<1, SInt32>>},
    {"HIThemeDrawButton", RR_Compose<RR_OutParam<4, HIRect>, RR_ScalarRval>,
     nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeButtonDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>,
                EX_OutParam<4, HIRect>>},
    {"HIThemeDrawFrame", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeFrameDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawGroupBox", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeGroupBoxDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawGrowBox", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIPoint>,
                EX_InParam<1, HIThemeGrowBoxDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawMenuBackground", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeMenuDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawMenuItem", RR_Compose<RR_OutParam<5, HIRect>, RR_ScalarRval>,
     nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIRect>,
                EX_InParam<2, HIThemeMenuItemDrawInfo>,
                EX_UpdateCFTypeArg<3>,
                EX_ScalarArg<4>,
                EX_OutParam<5, HIRect>>},
    {"HIThemeDrawMenuSeparator", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIRect>,
                EX_InParam<2, HIThemeMenuItemDrawInfo>,
                EX_UpdateCFTypeArg<3>,
                EX_ScalarArg<4>>},
    {"HIThemeDrawSeparator", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeSeparatorDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawTabPane", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIRect>,
                EX_InParam<1, HIThemeTabPaneDrawInfo>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeDrawTrack", RR_ScalarRval, nullptr,
     EX_Compose<EX_InParam<0, HIThemeTrackDrawInfo>,
                EX_InParam<1, HIRect>,
                EX_UpdateCFTypeArg<2>,
                EX_ScalarArg<3>>},
    {"HIThemeGetGrowBoxBounds",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, HIRect>>,
     nullptr,
     EX_Compose<EX_InParam<0, HIPoint>,
                EX_InParam<1, HIThemeGrowBoxDrawInfo>,
                EX_OutParam<2, HIRect>>},
    {"HIThemeSetFill", RR_ScalarRval, nullptr,
     EX_Compose<EX_ScalarArg<0>, EX_UpdateCFTypeArg<2>, EX_ScalarArg<3>>},
    {"IORegistryEntrySearchCFProperty", RR_ScalarRval},
    {"LSCopyAllHandlersForURLScheme", RR_ScalarRval},
    {"LSCopyApplicationForMIMEType",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, CFURLRef>>},
    {"LSCopyItemAttribute",
     RR_Compose<RR_ScalarRval, RR_OutParam<3, CFTypeRef>>},
    {"LSCopyKindStringForMIMEType",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, CFStringRef>>},
    {"LSGetApplicationForInfo",
     RR_Compose<RR_ScalarRval,
                RR_OutParam<4, FSRef>,
                RR_OutParam<5, CFURLRef>>},
    {"LSGetApplicationForURL",
     RR_Compose<RR_ScalarRval,
                RR_OutParam<2, FSRef>,
                RR_OutParam<3, CFURLRef>>},
    {"LSCopyDefaultApplicationURLForURL",
     RR_Compose<RR_ScalarRval, RR_OutParam<2, CFErrorRef>>},
    {"NSClassFromString", RR_ScalarRval, nullptr,
     EX_Compose<EX_CFTypeArg<0>, EX_CFTypeRval>},
    {"NSRectFill", nullptr, nullptr, EX_NoOp},
    {"NSSearchPathForDirectoriesInDomains", RR_ScalarRval},
    {"NSSetFocusRingStyle", nullptr, nullptr, EX_NoOp},
    {"NSTemporaryDirectory", RR_ScalarRval},
    {"OSSpinLockLock", nullptr, Preamble_OSSpinLockLock},
    {"PMCopyPageFormat", RR_ScalarRval},
    {"PMGetAdjustedPaperRect",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, PMRect>>},
    {"PMGetPageFormatPaper",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, PMPaper>>},
    {"PMPageFormatCreateDataRepresentation",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, CFDataRef>>},
    {"PMPageFormatCreateWithDataRepresentation",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, PMPageFormat>>},
    {"PMPaperGetMargins",
     RR_Compose<RR_ScalarRval, RR_OutParam<1, PMPaperMargins>>},
    {"ReleaseEvent", RR_ScalarRval},
    {"RemoveEventFromQueue", RR_ScalarRval},
    {"RetainEvent", RR_ScalarRval},
    {"SCDynamicStoreCopyProxies", RR_ScalarRval},
    {"SCDynamicStoreCreate", RR_ScalarRval},
    {"SCDynamicStoreCreateRunLoopSource", RR_ScalarRval},
    {"SCDynamicStoreKeyCreateProxies", RR_ScalarRval},
    {"SCDynamicStoreSetNotificationKeys", RR_ScalarRval},
    {"SecRandomCopyBytes", RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 1>>},
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
// Redirection Generation
///////////////////////////////////////////////////////////////////////////////

size_t NumRedirections() {
  return ArrayLength(gSystemRedirections);
}

static Redirection* gRedirections;

Redirection& GetRedirection(size_t aCallId) {
  MOZ_RELEASE_ASSERT(aCallId < ArrayLength(gSystemRedirections));
  return gRedirections[aCallId];
}

// Get the instruction pointer to use as the address of the base function for a
// redirection.
static uint8_t* FunctionStartAddress(Redirection& aRedirection) {
  uint8_t* addr =
      static_cast<uint8_t*>(dlsym(RTLD_DEFAULT, aRedirection.mName));
  if (!addr) {
    return nullptr;
  }

  if (addr[0] == 0xFF && addr[1] == 0x25) {
    return *(uint8_t**)(addr + 6 + *reinterpret_cast<int32_t*>(addr + 2));
  }

  return addr;
}

static bool PreserveCallerSaveRegisters(const char* aName) {
  // LLVM assumes that the call made when getting thread local variables will
  // preserve registers that are normally caller save. It's not clear what ABI
  // is actually assumed for this function so preserve all possible registers.
  return !strcmp(aName, "_tlv_bootstrap");
}

typedef std::unordered_map<std::string, void*> RedirectionAddressMap;
static RedirectionAddressMap* gRedirectionAddresses;

void InitializeRedirections() {
  size_t numSystemRedirections = ArrayLength(gSystemRedirections);
  gRedirections = new Redirection[numSystemRedirections];
  PodZero(gRedirections, numSystemRedirections);

  Maybe<Assembler> assembler;
  if (IsRecordingOrReplaying()) {
    static const size_t BufferSize = PageSize * 32;
    uint8_t* buffer = new uint8_t[BufferSize];
    UnprotectExecutableMemory(buffer, BufferSize);
    assembler.emplace(buffer, BufferSize);

    gRedirectionAddresses = new RedirectionAddressMap();
  }

  for (size_t i = 0; i < numSystemRedirections; i++) {
    const SystemRedirection& systemRedirection = gSystemRedirections[i];
    Redirection& redirection = gRedirections[i];

    redirection.mName = systemRedirection.mName;
    redirection.mSaveOutput = systemRedirection.mSaveOutput;
    redirection.mPreamble = systemRedirection.mPreamble;
    redirection.mExternalCall = systemRedirection.mExternalCall;
    redirection.mExternalPreamble = systemRedirection.mExternalPreamble;

    redirection.mOriginalFunction = FunctionStartAddress(redirection);

    if (IsRecordingOrReplaying()) {
      redirection.mRedirectedFunction =
          GenerateRedirectStub(assembler.ref(), i,
                               PreserveCallerSaveRegisters(redirection.mName));
      gRedirectionAddresses->insert(RedirectionAddressMap::value_type(
          std::string(redirection.mName), redirection.mRedirectedFunction));
    }
  }

  // Bind the gOriginal functions to their redirections' addresses.
#define INIT_ORIGINAL_FUNCTION(aName) \
  gOriginal_##aName = OriginalFunction(#aName);

  FOR_EACH_ORIGINAL_FUNCTION(INIT_ORIGINAL_FUNCTION)

#undef INIT_ORIGINAL_FUNCTION

  InitializeStaticClasses();
}

static void* FindRedirectionBinding(const char* aName) {
  RedirectionAddressMap::iterator iter = gRedirectionAddresses->find(aName);
  if (iter != gRedirectionAddresses->end()) {
    return iter->second;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Redirection Application
///////////////////////////////////////////////////////////////////////////////

// References to the binding function for thread local variables work
// differently from other external references. The function which gets the
// address of a TLV location is listed in the symbol tables as "_tlv_bootstrap",
// and while this function exists all it does is abort. dyld replaces references
// to this symbol with its own tlv_get_address function, which is not exported.
// We can still find tlv_get_address though by looking at the existing value
// when the _tlv_bootstrap reference is overwritten with our own binding.
static void* gTLVGetAddress;

static void ApplyBinding(const char* aName, void** aPtr) {
  if (aName[0] != '_') {
    return;
  }
  aName++;

  void* binding = FindRedirectionBinding(aName);
  if (binding) {
    if (!strcmp(aName, "_tlv_bootstrap")) {
      MOZ_RELEASE_ASSERT(gTLVGetAddress == nullptr ||
                         gTLVGetAddress == *aPtr);
      gTLVGetAddress = *aPtr;
    }
    *aPtr = binding;
  }
}

// Based on Apple's read_uleb128 implementation.
static uint64_t ReadLEB128(uint8_t*& aPtr, uint8_t* aEnd) {
  uint64_t result = 0;
  int bit = 0;
  do {
    MOZ_RELEASE_ASSERT(aPtr < aEnd);
    uint64_t slice = *aPtr & 0x7F;
    MOZ_RELEASE_ASSERT(bit <= 63);
    result |= (slice << bit);
    bit += 7;
  } while (*aPtr++ & 0x80);
  return result;
}

struct MachOLibrary {
  uint8_t* mAddress = nullptr;

  FileHandle mFile = 0;
  uint8_t* mFileAddress = nullptr;
  size_t mFileSize = 0;

  InfallibleVector<segment_command_64*> mSegments;

  nlist_64* mSymbolTable = nullptr;
  size_t mSymbolCount = 0;

  char* mStringTable = nullptr;
  uint32_t* mIndirectSymbols = nullptr;
  size_t mIndirectSymbolCount = 0;

  MachOLibrary(const char* aPath, uint8_t* aAddress) : mAddress(aAddress) {
    mFile = DirectOpenFile(aPath, /* aWriting */ false);

    struct stat info;
    int rv = fstat(mFile, &info);
    MOZ_RELEASE_ASSERT(rv >= 0);

    mFileSize = info.st_size;
    mFileAddress = (uint8_t*)mmap(nullptr, mFileSize, PROT_READ,
                                  MAP_PRIVATE, mFile, 0);
    MOZ_RELEASE_ASSERT(mFileAddress != MAP_FAILED);

    mach_header_64* header = (mach_header_64*)mFileAddress;
    MOZ_RELEASE_ASSERT(header->magic == MH_MAGIC_64);
  }

  ~MachOLibrary() {
    munmap(mFileAddress, mFileSize);
    DirectCloseFile(mFile);
  }

  void ApplyRedirections() {
    ForEachCommand([=](load_command* aCmd) {
        switch (aCmd->cmd & ~LC_REQ_DYLD) {
          case LC_SYMTAB:
            AddSymbolTable((symtab_command*)aCmd);
            break;
          case LC_DYSYMTAB:
            AddDynamicSymbolTable((dysymtab_command*)aCmd);
            break;
        }
      });

    ForEachCommand([=](load_command* aCmd) {
        switch (aCmd->cmd & ~LC_REQ_DYLD) {
          case LC_DYLD_INFO:
            BindLoadInfo((dyld_info_command*)aCmd);
            break;
          case LC_SEGMENT_64:  {
            auto ncmd = (segment_command_64*)aCmd;
            mSegments.append(ncmd);
            BindSegment(ncmd);
            break;
          }
        }
      });
  }

  void ForEachCommand(const std::function<void(load_command*)>& aCallback) {
    mach_header_64* header = (mach_header_64*)mFileAddress;
    uint32_t offset = sizeof(mach_header_64);
    for (size_t i = 0; i < header->ncmds; i++) {
      load_command* cmd = (load_command*) (mFileAddress + offset);
      aCallback(cmd);
      offset += cmd->cmdsize;
    }
  }

  void AddSymbolTable(symtab_command* aCmd) {
    mSymbolTable = (nlist_64*) (mFileAddress + aCmd->symoff);
    mSymbolCount = aCmd->nsyms;

    mStringTable = (char*) (mFileAddress + aCmd->stroff);
  }

  void AddDynamicSymbolTable(dysymtab_command* aCmd) {
    mIndirectSymbols = (uint32_t*) (mFileAddress + aCmd->indirectsymoff);
    mIndirectSymbolCount = aCmd->nindirectsyms;
  }

  void BindLoadInfo(dyld_info_command* aCmd) {
    DoBindings(aCmd->bind_off, aCmd->bind_size);
    DoBindings(aCmd->weak_bind_off, aCmd->weak_bind_size);
    DoBindings(aCmd->lazy_bind_off, aCmd->lazy_bind_size);
  }

  void DoBindings(size_t aStart, size_t aSize) {
    size_t segmentIndex = 0;
    size_t segmentOffset = 0;
    const char* symbolName;
    uint8_t symbolFlags = 0;
    uint8_t symbolType = BIND_TYPE_POINTER;

    uint8_t* cursor = mFileAddress + aStart;
    uint8_t* end = cursor + aSize;
    while (cursor < end) {
      uint8_t opcode = *cursor & BIND_OPCODE_MASK;
      uint8_t immediate = *cursor & BIND_IMMEDIATE_MASK;
      cursor++;

      switch (opcode) {
        case BIND_OPCODE_DONE:
          return;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
        case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
          break;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
          ReadLEB128(cursor, end);
          break;
        case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
          segmentIndex = immediate;
          segmentOffset = ReadLEB128(cursor, end);
          break;
        case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
          symbolName = (char*)cursor;
          symbolFlags = immediate;
          cursor += strlen(symbolName) + 1;
          break;
        case BIND_OPCODE_SET_TYPE_IMM:
          symbolType = immediate;
          break;
        case BIND_OPCODE_SET_ADDEND_SLEB:
          break;
        case BIND_OPCODE_DO_BIND:
          DoBinding(segmentIndex, segmentOffset,
                    symbolName, symbolFlags, symbolType);
          segmentOffset += sizeof(uintptr_t);
          break;
        case BIND_OPCODE_ADD_ADDR_ULEB:
          segmentOffset += ReadLEB128(cursor, end);
          break;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
          DoBinding(segmentIndex, segmentOffset,
                    symbolName, symbolFlags, symbolType);
          segmentOffset += ReadLEB128(cursor, end) + sizeof(uintptr_t);
          break;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
          DoBinding(segmentIndex, segmentOffset,
                    symbolName, symbolFlags, symbolType);
          segmentOffset += immediate * sizeof(uintptr_t) + sizeof(uintptr_t);
          break;
        case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
          size_t count = ReadLEB128(cursor, end);
          size_t skip = ReadLEB128(cursor, end);
          for (size_t i = 0; i < count; i++) {
            DoBinding(segmentIndex, segmentOffset,
                      symbolName, symbolFlags, symbolType);
            segmentOffset += skip + sizeof(uintptr_t);
          }
          break;
        }
        default:
          MOZ_CRASH("Unknown binding opcode");
      }
    }
  }

  void DoBinding(size_t aSegmentIndex, size_t aSegmentOffset,
                 const char* aSymbolName, uint8_t aSymbolFlags,
                 uint8_t aSymbolType) {
    MOZ_RELEASE_ASSERT(aSegmentIndex < mSegments.length());
    segment_command_64* seg = mSegments[aSegmentIndex];

    uint8_t* address = mAddress + seg->vmaddr + aSegmentOffset;

    MOZ_RELEASE_ASSERT(aSymbolType == BIND_TYPE_POINTER);
    void** ptr = (void**) address;
    ApplyBinding(aSymbolName, ptr);
  }

  void BindSegment(segment_command_64* aCmd) {
    section_64* sect = (section_64*) (aCmd + 1);
    for (size_t i = 0; i < aCmd->nsects; i++, sect++) {
      switch (sect->flags & SECTION_TYPE) {
        case S_NON_LAZY_SYMBOL_POINTERS:
        case S_LAZY_SYMBOL_POINTERS: {
          size_t entryCount = sect->size / 8;
          size_t symbolStart = sect->reserved1;
          for (size_t i = 0; i < entryCount; i++) {
            void* ptr = mAddress + sect->addr + i * 8;
            MOZ_RELEASE_ASSERT(symbolStart + i < mIndirectSymbolCount);
            uint32_t symbolIndex = mIndirectSymbols[symbolStart + i];
            if (symbolIndex == INDIRECT_SYMBOL_ABS ||
                symbolIndex == INDIRECT_SYMBOL_LOCAL) {
              continue;
            }
            MOZ_RELEASE_ASSERT(symbolIndex < mSymbolCount);
            const nlist_64& sym = mSymbolTable[symbolIndex];
            char* str = &mStringTable[sym.n_un.n_strx];
            ApplyBinding(str, (void**)ptr);
          }
          break;
        }
      }
    }
  }
};

// One symbol from each library that is loaded at startup and has external
// references we want to fix up. These libraries might not be loaded at startup
// so we check again after each new library is loaded, and clear out these
// entries once we have fixed their external references.
static const char* gInitialLibrarySymbols[] = {
  "RecordReplayInterface_Initialize", // XUL
  "_ZN7mozilla12recordreplay10InitializeEiPPc", // libmozglue
  "PR_Initialize", // libnss3
  "FREEBL_GetVector", // libfreebl
  "NSC_GetFunctionList", // libsoftokn3
};

void ApplyLibraryRedirections(void* aLibrary) {
  for (const char*& symbol : gInitialLibrarySymbols) {
    if (!symbol) {
      continue;
    }

    void* ptr = dlsym(aLibrary ? aLibrary : RTLD_DEFAULT, symbol);
    if (!ptr) {
      continue;
    }

    Dl_info info;
    dladdr(ptr, &info);
    uint8_t* address = (uint8_t*)info.dli_fbase;

    MachOLibrary library(info.dli_fname, address);
    library.ApplyRedirections();

    symbol = nullptr;
  }

  static bool fixedBootstrap = false;
  if (!fixedBootstrap) {
    fixedBootstrap = true;
    for (size_t i = 0; i < NumRedirections(); i++) {
      Redirection& redirection = gRedirections[i];
      if (!strcmp(redirection.mName, "_tlv_bootstrap")) {
        redirection.mOriginalFunction = (uint8_t*)gTLVGetAddress;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Platform Symbols
///////////////////////////////////////////////////////////////////////////////

// Platform symbols are exported and used when we will be replaying recordings
// on another platform. Special constants which can be passed to system library
// functions are described here and read by the linker managing the replay,
// so that those constants can be converted to appropriate values on the host
// platform.

struct PlatformSymbol {
  const char* mName;
  uint32_t mValue;
};

#define NewPlatformSymbol(Name) { #Name, Name }

extern "C" {

MOZ_EXPORT PlatformSymbol RecordReplayInterface_PlatformSymbols[] = {
  NewPlatformSymbol(PROT_READ),
  NewPlatformSymbol(PROT_WRITE),
  NewPlatformSymbol(PROT_EXEC),
  NewPlatformSymbol(MAP_FIXED),
  NewPlatformSymbol(MAP_PRIVATE),
  NewPlatformSymbol(MAP_ANON),
  NewPlatformSymbol(MAP_NORESERVE),
  NewPlatformSymbol(PTHREAD_MUTEX_NORMAL),
  NewPlatformSymbol(PTHREAD_MUTEX_RECURSIVE),
  NewPlatformSymbol(PTHREAD_CREATE_JOINABLE),
  NewPlatformSymbol(PTHREAD_CREATE_DETACHED),
  NewPlatformSymbol(SYS_thread_selfid),
  NewPlatformSymbol(_SC_PAGESIZE),
  NewPlatformSymbol(_SC_NPROCESSORS_CONF),
  NewPlatformSymbol(RLIMIT_AS),
  NewPlatformSymbol(ETIMEDOUT),
  NewPlatformSymbol(EINTR),
  NewPlatformSymbol(EPIPE),
  NewPlatformSymbol(EAGAIN),
  NewPlatformSymbol(ECONNRESET),
  { nullptr, 0 },
};

} // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Direct system call API
///////////////////////////////////////////////////////////////////////////////

const char* SymbolNameRaw(void* aPtr) {
  Dl_info info;
  return (dladdr(aPtr, &info) && info.dli_sname) ? info.dli_sname : "???";
}

void* DirectAllocateMemory(size_t aSize) {
  void* res = CallFunction<void*>(gOriginal_mmap, nullptr, aSize,
                                  PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_ANON | MAP_PRIVATE, -1, 0);
  MOZ_RELEASE_ASSERT(res && res != (void*)-1);
  return res;
}

void DirectDeallocateMemory(void* aAddress, size_t aSize) {
  ssize_t rv = munmap(aAddress, aSize);
  MOZ_RELEASE_ASSERT(rv >= 0);
}

void DirectMakeInaccessible(void* aAddress, size_t aSize) {
  ssize_t rv = mprotect(aAddress, aSize, PROT_NONE);
  MOZ_RELEASE_ASSERT(rv == 0);
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

static double gNsPerTick;

void InitializeCurrentTime() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  gNsPerTick = (double)info.numer / info.denom;
}

double CurrentTime() {
  return CallFunction<int64_t>(gOriginal_mach_absolute_time) *
         gNsPerTick / 1000.0;
}

NativeThreadId DirectSpawnThread(void (*aFunction)(void*), void* aArgument,
                                 void* aStackBase, size_t aStackSize) {
  MOZ_RELEASE_ASSERT(!IsRecordingOrReplaying() ||
                     AreThreadEventsPassedThrough());

  pthread_attr_t attr;
  int rv = pthread_attr_init(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);

  if (aStackBase) {
    rv = pthread_attr_setstack(&attr, aStackBase, aStackSize);
    MOZ_RELEASE_ASSERT(rv == 0);
  }

  rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  MOZ_RELEASE_ASSERT(rv == 0);

  if (!gOriginal_pthread_create) {
    gOriginal_pthread_create = BitwiseCast<void*>(pthread_create);
  }

  pthread_t pthread;
  rv = CallFunction<int>(gOriginal_pthread_create, &pthread, &attr,
                         (void* (*)(void*))aFunction, aArgument);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_destroy(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);

  return pthread;
}

NativeThreadId DirectCurrentThread() {
  return CallFunction<pthread_t>(gOriginal_pthread_self);
}

}  // namespace recordreplay
}  // namespace mozilla
