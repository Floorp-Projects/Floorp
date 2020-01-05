/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ProcessRedirect_h
#define mozilla_recordreplay_ProcessRedirect_h

#include "Assembler.h"
#include "Callback.h"
#include "CallFunction.h"
#include "ProcessRecordReplay.h"
#include "ProcessRewind.h"
#include "Thread.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Casting.h"

#include <errno.h>

namespace mozilla {
namespace recordreplay {

// Redirections Overview.
//
// The vast majority of recording and replaying is done through function
// redirections. When the record/replay system is initialized, a set of system
// library API functions have their machine code modified so that when that API
// is called it redirects control to a custom record/replay function with the
// same signature. Machine code is also generated that contains any overwritten
// instructions in the API, and which may be called to get the API's original
// behavior before it was redirected.
//
// In the usual case, a record/replay function redirection does the following
// standard steps:
//
// 1. If events are being passed through, the original function is called and
//    its results returned to the caller, as if the redirection was not there
//    at all.
//
// 2. If events are not passed through and we are recording, the original
//    function is called, and then an event is recorded for the current thread
//    along with any outputs produced by the call.
//
// 3. If events are not passed through and we are replaying, the original
//    function is *not* called, but rather the event and outputs are read from
//    the recording and sent back to the caller.
//
// Redirections do not need to adhere to this protocol, however, and can have
// whatever behaviors that are necessary for reliable record/replay.
// Controlling how a redirected function behaves and which outputs it saves is
// done through a set of hooks associated with each redirection.
//
// Some platforms need additional redirection techniques for handling different
// features of that platform. See the individual ProcessRedirect*.cpp files for
// descriptions of these.
//
// The main advantage of using redirections is that Gecko code does not need to
// be modified at all to take advantage of them. Redirected APIs should be
// functions that are directly called by Gecko code and are part of system
// libraries. These APIs are well defined, well documented by the platform, and
// stable. The main maintenance burden going forward is in handling new library
// APIs that were not previously called by Gecko.
//
// The main risk with using function redirections is that the set of redirected
// functions is incomplete. If a library API is not redirected then it might
// behave differently between recording and replaying, or it might crash while
// replaying.

///////////////////////////////////////////////////////////////////////////////
// Function Redirections
///////////////////////////////////////////////////////////////////////////////

// Capture the arguments that can be passed to a redirection, and provide
// storage to specify the redirection's return value. We only need to capture
// enough argument data here for calls made directly from Gecko code,
// i.e. where events are not passed through. Calls made while events are passed
// through are performed with the same stack and register state as when they
// were initially invoked.
//
// Arguments and return value indexes refer to the register contents as passed
// to the function originally. For functions with complex or floating point
// arguments and return values, the right index to use might be different than
// expected, per the requirements of the System V x64 ABI.
struct CallArguments {
  // The maximum number of stack arguments that can be captured.
  static const size_t NumStackArguments = 64;

 protected:
  size_t arg0;        // 0
  size_t arg1;        // 8
  size_t arg2;        // 16
  size_t arg3;        // 24
  size_t arg4;        // 32
  size_t arg5;        // 40
  double floatarg0;   // 48
  double floatarg1;   // 56
  double floatarg2;   // 64
  size_t rval0;       // 72
  size_t rval1;       // 80
  double floatrval0;  // 88
  double floatrval1;  // 96
  size_t stack[NumStackArguments];  // 104
                                    // Size: 616

 public:
  template <typename T>
  T& Arg(size_t aIndex) {
    static_assert(sizeof(T) == sizeof(size_t), "Size must match");
    static_assert(IsFloatingPoint<T>::value == false, "Use FloatArg");
    MOZ_RELEASE_ASSERT(aIndex < 70);
    switch (aIndex) {
      case 0:
        return (T&)arg0;
      case 1:
        return (T&)arg1;
      case 2:
        return (T&)arg2;
      case 3:
        return (T&)arg3;
      case 4:
        return (T&)arg4;
      case 5:
        return (T&)arg5;
      default:
        return (T&)stack[aIndex - 6];
    }
  }

  template <size_t Index, typename T>
  T& Arg() {
    return Arg<T>(Index);
  }

  template <size_t Index>
  double& FloatArg() {
    static_assert(Index < 3, "Bad index");
    switch (Index) {
      case 0:
        return floatarg0;
      case 1:
        return floatarg1;
      case 2:
        return floatarg2;
    }
  }

  template <size_t Offset>
  size_t* StackAddress() {
    static_assert(Offset % sizeof(size_t) == 0, "Bad stack offset");
    return &stack[Offset / sizeof(size_t)];
  }

  template <typename T, size_t Index = 0>
  T& Rval() {
    static_assert(sizeof(T) == sizeof(size_t), "Size must match");
    static_assert(IsFloatingPoint<T>::value == false, "Use FloatRval instead");
    static_assert(Index == 0 || Index == 1, "Bad index");
    switch (Index) {
      case 0:
        return (T&)rval0;
      case 1:
        return (T&)rval1;
    }
  }

  template <size_t Index = 0>
  double& FloatRval() {
    static_assert(Index == 0 || Index == 1, "Bad index");
    switch (Index) {
      case 0:
        return floatrval0;
      case 1:
        return floatrval1;
    }
  }
};

// Generic type for a system error code.
typedef ssize_t ErrorType;

// Signature for a function that saves some output produced by a redirection
// while recording, and restores that output while replaying. aEvents is the
// event stream for the current thread, aArguments specifies the arguments to
// the called function, and aError specifies any system error which the call
// produces.
typedef void (*SaveOutputFn)(Stream& aEvents, CallArguments* aArguments,
                             ErrorType* aError);

// Possible results for the redirection preamble hook.
enum class PreambleResult {
  // Do not perform any further processing.
  Veto,

  // If events are not passed through, add an event for the function call and
  // perform a function redirection as normal.
  Redirect,

  // Don't add an event for the call or perform a normal function redirection.
  // If events are not passed through, they may still be added for transitive
  // callees.
  IgnoreRedirect,

  // Do not add an event for the call or anything it transitively calls, as if
  // events were passed through.
  PassThrough,
};

// Signature for a function that is called on entry to a redirection and can
// modify its behavior.
typedef PreambleResult (*PreambleFn)(CallArguments* aArguments);

// Signature for a function that conveys data about a call to or from an
// external process.
struct ExternalCallContext;
typedef void (*ExternalCallFn)(ExternalCallContext& aCx);

// Information about a system library API function which is being redirected.
struct Redirection {
  // Name of the function being redirected.
  const char* mName;

  // Pointer to the original function being redirected.
  uint8_t* mOriginalFunction;

  // Pointer to a stub for this redirection that calls its hooks.
  uint8_t* mRedirectedFunction;

  // Redirection hooks are used to control the behavior of the redirected
  // function.

  // If specified, will be called at the end of the redirection when events are
  // not being passed through to record/replay any outputs from the call.
  SaveOutputFn mSaveOutput;

  // If specified, will be called upon entry to the redirected call.
  PreambleFn mPreamble;

  // If specified, allows this call to be made after diverging from the
  // recording. See ExternalCall.h
  ExternalCallFn mExternalCall;

  // Additional preamble that is only called while replaying and diverged from
  // the recording.
  PreambleFn mExternalPreamble;
};

// Platform specific methods describing the set of redirections.
size_t NumRedirections();
Redirection& GetRedirection(size_t aCallId);

// Platform specific initialization of redirections. This is done on both
// recording/replaying and middleman processes, and allows OriginalCall() to
// work in either case.
void InitializeRedirections();

// Platform specific routine to apply redirections to external references in
// all initially loaded libraries.
void ApplyInitialLibraryRedirections();

// Generate a stub that will call the hooks associated with a redirection.
uint8_t* GenerateRedirectStub(Assembler& aAssembler, size_t aCallId,
                              bool aPreserveCallerSaveRegisters);

// Functions for saving or restoring system error codes.
static inline ErrorType SaveError() { return errno; }
static inline void RestoreError(ErrorType aError) { errno = aError; }

// Specify the default ABI to use by the record/replay macros below.
#define DEFAULTABI

// Define CallFunction(...) for all supported ABIs.
DefineAllCallFunctions(DEFAULTABI)

    // Get the address of the original function for a call event ID.
    static inline void* OriginalFunction(size_t aCallId) {
  return GetRedirection(aCallId).mOriginalFunction;
}

// Get the address of the original function by name.
void* OriginalFunction(const char* aName);

static inline ThreadEvent CallIdToThreadEvent(size_t aCallId) {
  return (ThreadEvent)((uint32_t)ThreadEvent::CallStart + aCallId);
}

void RecordReplayInvokeCall(void* aFunction, CallArguments* aArguments);

///////////////////////////////////////////////////////////////////////////////
// Callback Redirections
///////////////////////////////////////////////////////////////////////////////

// Below are helpers for use in handling a common callback pattern used within
// redirections: the system is passed a pointer to a Gecko callback, and a
// pointer to some opaque Gecko data which the system will pass to the callback
// when invoking it.
//
// This pattern may be handled by replacing the Gecko callback with a callback
// wrapper (see Callback.h), and replacing the opaque Gecko data with a pointer
// to a CallbackWrapperData structure, which contains both the original Gecko
// callback to use and the data which should be passed to it.
//
// The RecordReplayCallback is used early in the callback wrapper to save and
// restore both the Gecko callback and its opaque data pointer.

struct CallbackWrapperData {
  void* mFunction;
  void* mData;

  template <typename FunctionType>
  CallbackWrapperData(FunctionType aFunction, void* aData)
      : mFunction(BitwiseCast<void*>(aFunction)), mData(aData) {}
};

// This class should not be used directly, but rather through the macro below.
template <typename FunctionType>
struct AutoRecordReplayCallback {
  FunctionType mFunction;

  AutoRecordReplayCallback(void** aDataArgument, size_t aCallbackId)
      : mFunction(nullptr) {
    MOZ_ASSERT(IsRecordingOrReplaying());
    if (IsRecording()) {
      CallbackWrapperData* wrapperData = (CallbackWrapperData*)*aDataArgument;
      mFunction = (FunctionType)wrapperData->mFunction;
      *aDataArgument = wrapperData->mData;
      BeginCallback(aCallbackId);
    }
    SaveOrRestoreCallbackData((void**)&mFunction);
    SaveOrRestoreCallbackData(aDataArgument);
  }

  ~AutoRecordReplayCallback() {
    if (IsRecording()) {
      EndCallback();
    }
  }
};

// Macro for using AutoRecordReplayCallback.
#define RecordReplayCallback(aFunctionType, aDataArgument)   \
  AutoRecordReplayCallback<aFunctionType> rrc(aDataArgument, \
                                              CallbackEvent_##aFunctionType)

///////////////////////////////////////////////////////////////////////////////
// Redirection Helpers
///////////////////////////////////////////////////////////////////////////////

extern Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve>
    gMemoryLeakBytes;

// For allocating memory in redirections that will never be reclaimed. This is
// done for simplicity. If the amount of leaked memory from redirected calls
// grows too large then steps can be taken to more closely emulate the library
// behavior.
template <typename T>
static inline T* NewLeakyArray(size_t aSize) {
  gMemoryLeakBytes += aSize * sizeof(T);
  return new T[aSize];
}

// Record/replay a string rval.
static inline void RR_CStringRval(Stream& aEvents, CallArguments* aArguments,
                                  ErrorType* aError) {
  auto& rval = aArguments->Rval<char*>();
  size_t len = (IsRecording() && rval) ? strlen(rval) + 1 : 0;
  aEvents.RecordOrReplayValue(&len);
  if (IsReplaying()) {
    // Note: Some users (e.g. realpath) require malloc to be used to allocate
    // the returned buffer.
    rval = len ? (char*)malloc(len) : nullptr;
  }
  if (len) {
    aEvents.RecordOrReplayBytes(rval, len);
  }
}

// Record/replay a fixed size rval buffer.
template <size_t ByteCount>
static inline void RR_RvalBuffer(Stream& aEvents, CallArguments* aArguments,
                                 ErrorType* aError) {
  auto& rval = aArguments->Rval<void*>();
  bool hasRval = IsRecording() && rval;
  aEvents.RecordOrReplayValue(&hasRval);
  if (IsReplaying()) {
    rval = hasRval ? NewLeakyArray<char>(ByteCount) : nullptr;
  }
  if (hasRval) {
    aEvents.RecordOrReplayBytes(rval, ByteCount);
  }
}

// Ensure that the return value matches the specified argument.
template <size_t Argument>
static inline void RR_RvalIsArgument(Stream& aEvents, CallArguments* aArguments,
                                     ErrorType* aError) {
  auto& rval = aArguments->Rval<size_t>();
  auto& arg = aArguments->Arg<Argument, size_t>();
  if (IsRecording()) {
    MOZ_RELEASE_ASSERT(rval == arg);
  } else {
    rval = arg;
  }
}

// No-op record/replay output method.
static inline void RR_NoOp(Stream& aEvents, CallArguments* aArguments,
                           ErrorType* aError) {}

// Record/replay multiple SaveOutput functions sequentially.
template <SaveOutputFn Fn0, SaveOutputFn Fn1, SaveOutputFn Fn2 = RR_NoOp,
          SaveOutputFn Fn3 = RR_NoOp, SaveOutputFn Fn4 = RR_NoOp>
static inline void RR_Compose(Stream& aEvents, CallArguments* aArguments,
                              ErrorType* aError) {
  Fn0(aEvents, aArguments, aError);
  Fn1(aEvents, aArguments, aError);
  Fn2(aEvents, aArguments, aError);
  Fn3(aEvents, aArguments, aError);
  Fn4(aEvents, aArguments, aError);
}

// Record/replay a success code rval (where negative values are failure) and
// errno value on failure. SuccessFn does any further processing in non-error
// cases.
template <SaveOutputFn SuccessFn = RR_NoOp>
static inline void RR_SaveRvalHadErrorNegative(Stream& aEvents,
                                               CallArguments* aArguments,
                                               ErrorType* aError) {
  auto& rval = aArguments->Rval<ssize_t>();
  aEvents.RecordOrReplayValue(&rval);
  if (rval < 0) {
    aEvents.RecordOrReplayValue(aError);
  } else {
    SuccessFn(aEvents, aArguments, aError);
  }
}

// Record/replay a success code rval (where zero is a failure) and errno value
// on failure. SuccessFn does any further processing in non-error cases.
template <SaveOutputFn SuccessFn = RR_NoOp>
static inline void RR_SaveRvalHadErrorZero(Stream& aEvents,
                                           CallArguments* aArguments,
                                           ErrorType* aError) {
  auto& rval = aArguments->Rval<ssize_t>();
  aEvents.RecordOrReplayValue(&rval);
  if (rval == 0) {
    aEvents.RecordOrReplayValue(aError);
  } else {
    SuccessFn(aEvents, aArguments, aError);
  }
}

// Record/replay the contents of a buffer at argument BufferArg with element
// count at CountArg.
template <size_t BufferArg, size_t CountArg, typename ElemType = char>
static inline void RR_WriteBuffer(Stream& aEvents, CallArguments* aArguments,
                                  ErrorType* aError) {
  auto& buf = aArguments->Arg<BufferArg, ElemType*>();
  auto& count = aArguments->Arg<CountArg, size_t>();
  aEvents.CheckInput(count);
  aEvents.RecordOrReplayBytes(buf, count * sizeof(ElemType));
}

// Record/replay the contents of a buffer at argument BufferArg with element
// count at CountArg, and which may be null.
template <size_t BufferArg, size_t CountArg, typename ElemType = char>
static inline void RR_WriteOptionalBuffer(Stream& aEvents,
                                          CallArguments* aArguments,
                                          ErrorType* aError) {
  auto& buf = aArguments->Arg<BufferArg, ElemType*>();
  auto& count = aArguments->Arg<CountArg, size_t>();
  aEvents.CheckInput(!!buf);
  if (buf) {
    aEvents.CheckInput(count);
    aEvents.RecordOrReplayBytes(buf, count * sizeof(ElemType));
  }
}

// Record/replay the contents of a buffer at argument BufferArg with fixed
// size ByteCount.
template <size_t BufferArg, size_t ByteCount>
static inline void RR_WriteBufferFixedSize(Stream& aEvents,
                                           CallArguments* aArguments,
                                           ErrorType* aError) {
  auto& buf = aArguments->Arg<BufferArg, void*>();
  aEvents.RecordOrReplayBytes(buf, ByteCount);
}

// Record/replay the contents of a buffer at argument BufferArg with fixed
// size ByteCount, and which may be null.
template <size_t BufferArg, size_t ByteCount>
static inline void RR_WriteOptionalBufferFixedSize(Stream& aEvents,
                                                   CallArguments* aArguments,
                                                   ErrorType* aError) {
  auto& buf = aArguments->Arg<BufferArg, void*>();
  aEvents.CheckInput(!!buf);
  if (buf) {
    aEvents.RecordOrReplayBytes(buf, ByteCount);
  }
}

// Record/replay an out parameter.
template <size_t Arg, typename Type>
static inline void RR_OutParam(Stream& aEvents, CallArguments* aArguments,
                               ErrorType* aError) {
  RR_WriteOptionalBufferFixedSize<Arg, sizeof(Type)>(aEvents, aArguments,
                                                     aError);
}

// Record/replay the contents of a buffer at argument BufferArg with byte size
// CountArg, where the call return value plus Offset indicates the amount of
// data written to the buffer by the call. The return value must already have
// been recorded/replayed.
template <size_t BufferArg, size_t CountArg, size_t Offset = 0>
static inline void RR_WriteBufferViaRval(Stream& aEvents,
                                         CallArguments* aArguments,
                                         ErrorType* aError) {
  auto& buf = aArguments->Arg<BufferArg, void*>();
  auto& count = aArguments->Arg<CountArg, size_t>();
  aEvents.CheckInput(count);

  auto& rval = aArguments->Rval<size_t>();
  MOZ_RELEASE_ASSERT(rval + Offset <= count);
  aEvents.RecordOrReplayBytes(buf, rval + Offset);
}

// Record/replay a scalar return value.
static inline void RR_ScalarRval(Stream& aEvents, CallArguments* aArguments,
                                 ErrorType* aError) {
  aEvents.RecordOrReplayValue(&aArguments->Rval<size_t>());
}

// Record/replay a complex scalar return value that fits in two registers.
static inline void RR_ComplexScalarRval(Stream& aEvents,
                                        CallArguments* aArguments,
                                        ErrorType* aError) {
  aEvents.RecordOrReplayValue(&aArguments->Rval<size_t>());
  aEvents.RecordOrReplayValue(&aArguments->Rval<size_t, 1>());
}

// Record/replay a floating point return value.
static inline void RR_FloatRval(Stream& aEvents, CallArguments* aArguments,
                                ErrorType* aError) {
  aEvents.RecordOrReplayValue(&aArguments->FloatRval());
}

// Record/replay a complex floating point return value that fits in two
// registers.
static inline void RR_ComplexFloatRval(Stream& aEvents,
                                       CallArguments* aArguments,
                                       ErrorType* aError) {
  aEvents.RecordOrReplayValue(&aArguments->FloatRval());
  aEvents.RecordOrReplayValue(&aArguments->FloatRval<1>());
}

// Record/replay a return value that does not fit in the return registers,
// and whose storage is pointed to by the first argument register.
template <size_t ByteCount>
static inline void RR_OversizeRval(Stream& aEvents, CallArguments* aArguments,
                                   ErrorType* aError) {
  RR_WriteBufferFixedSize<0, ByteCount>(aEvents, aArguments, aError);
}

template <size_t ReturnValue>
static inline PreambleResult Preamble_Veto(CallArguments* aArguments) {
  aArguments->Rval<size_t>() = ReturnValue;
  return PreambleResult::Veto;
}

template <size_t ReturnValue>
static inline PreambleResult Preamble_VetoIfNotPassedThrough(
    CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::PassThrough;
  }
  aArguments->Rval<size_t>() = ReturnValue;
  return PreambleResult::Veto;
}

static inline PreambleResult Preamble_PassThrough(CallArguments* aArguments) {
  return PreambleResult::PassThrough;
}

static inline PreambleResult Preamble_WaitForever(CallArguments* aArguments) {
  Thread::WaitForever();
  Unreachable();
  return PreambleResult::PassThrough;
}

static inline PreambleResult Preamble_NYI(CallArguments* aArguments) {
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::PassThrough;
  }
  MOZ_CRASH("Redirection NYI");
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// Other Redirection Interfaces
///////////////////////////////////////////////////////////////////////////////

// Given an argument function aFunction, generate code for a new function that
// takes one fewer argument than aFunction and then calls aFunction with all
// its arguments and the aArgument value in the last argument position.
//
// i.e. if aFunction has the signature: size_t (*)(void*, void*, void*);
//
// Then BindFunctionArgument(aFunction, aArgument, 2) produces this function:
//
// size_t result(void* a0, void* a1) {
//   return aFunction(a0, a1, aArgument);
// }
//
// Supported positions for the bound argument are 1, 2, and 3.
void* BindFunctionArgument(void* aFunction, void* aArgument,
                           size_t aArgumentPosition, Assembler& aAssembler);

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ProcessRedirect_h
