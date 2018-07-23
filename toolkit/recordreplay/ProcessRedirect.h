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
#include "ipc/Channel.h"

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
// Macros are provided below to streamline this process. Redirections do not
// need to adhere to this protocol, however, and can have whatever behaviors
// that are necessary for reliable record/replay.
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

// Information about a system library API function which is being redirected.
struct Redirection
{
  // Name of the function being redirected.
  const char* mName;

  // Address of the function which is being redirected. The code for this
  // function is modified so that attempts to call this function will instead
  // call mNewFunction.
  uint8_t* mBaseFunction;

  // Function with the same signature as mBaseFunction, which may have
  // different behavior for recording/replaying the call.
  uint8_t* mNewFunction;

  // Function with the same signature and original behavior as
  // mBaseFunction.
  uint8_t* mOriginalFunction;
};

// All platform specific redirections, indexed by the call event.
extern Redirection gRedirections[];

// Do early initialization of redirections. This is done on both
// recording/replaying and middleman processes, and allows OriginalCall() to
// work in either case.
void EarlyInitializeRedirections();

// Set up all platform specific redirections, or fail and set
// gInitializationFailureMessage.
bool InitializeRedirections();

// Generic type for a system error code.
typedef ssize_t ErrorType;

// Functions for saving or restoring system error codes.
static inline ErrorType SaveError() { return errno; }
static inline void RestoreError(ErrorType aError) { errno = aError; }

// Specify the default ABI to use by the record/replay macros below.
#define DEFAULTABI

// Define CallFunction(...) for all supported ABIs.
DefineAllCallFunctions(DEFAULTABI)

// Get the address of the original function for a call event ID.
static inline void*
OriginalFunction(size_t aCallId)
{
  return gRedirections[aCallId].mOriginalFunction;
}

#define TokenPaste(aFirst, aSecond) aFirst ## aSecond

// Call the original function for a call event ID with a particular ABI and any
// number of arguments.
#define OriginalCallABI(aName, aReturnType, aABI, ...)          \
  TokenPaste(CallFunction, aABI) <aReturnType>                  \
    (OriginalFunction(CallEvent_ ##aName), ##__VA_ARGS__)

// Call the original function for a call event ID with the default ABI.
#define OriginalCall(aName, aReturnType, ...)                   \
  OriginalCallABI(aName, aReturnType, DEFAULTABI, ##__VA_ARGS__)

// State for a function redirection which performs the standard steps (see the
// comment at the start of this file). This should not be created directly, but
// rather through one of the macros below.
struct AutoRecordReplayFunctionVoid
{
  // The current thread, or null if events are being passed through.
  Thread* mThread;

  // Any system error generated by the call which was redirected.
  ErrorType mError;

protected:
  // Information about the call being recorded.
  size_t mCallId;
  const char* mCallName;

public:
  AutoRecordReplayFunctionVoid(size_t aCallId, const char* aCallName)
    : mThread(AreThreadEventsPassedThrough() ? nullptr : Thread::Current()),
      mError(0), mCallId(aCallId), mCallName(aCallName)
  {
    if (mThread) {
      // Calling any redirection which performs the standard steps will cause
      // debugger operations that have diverged from the recording to fail.
      EnsureNotDivergedFromRecording();

      MOZ_ASSERT(!AreThreadEventsDisallowed());

      // Pass through events in case we are calling the original function.
      mThread->SetPassThrough(true);
    }
  }

  ~AutoRecordReplayFunctionVoid()
  {
    if (mThread) {
      // Restore any error saved or replayed earlier to the system.
      RestoreError(mError);
    }
  }

  // Begin recording or replaying data for the call. This must be called before
  // destruction if mThread is non-null.
  inline void StartRecordReplay() {
    MOZ_ASSERT(mThread);

    // Save any system error in case we want to record/replay it.
    mError = SaveError();

    // Stop the event passing through that was initiated in the constructor.
    mThread->SetPassThrough(false);

    // Add an event for the thread.
    RecordReplayAssert("%s", mCallName);
    ThreadEvent ev = (ThreadEvent)((uint32_t)ThreadEvent::CallStart + mCallId);
    mThread->Events().RecordOrReplayThreadEvent(ev);
  }
};

// State for a function redirection that performs the standard steps and also
// returns a value.
template <typename ReturnType>
struct AutoRecordReplayFunction : AutoRecordReplayFunctionVoid
{
  // The value which this function call should return.
  ReturnType mRval;

  AutoRecordReplayFunction(size_t aCallId, const char* aCallName)
    : AutoRecordReplayFunctionVoid(aCallId, aCallName)
  {}
};

// Macros for recording or replaying a function that performs the standard
// steps. These macros should be used near the start of the body of a
// redirection function, and will fall through only if events are not
// passed through and the outputs of the function need to be recorded or
// replayed.
//
// These macros define an AutoRecordReplayFunction local |rrf| with state for
// the redirection, and additional locals |events| and (if the function has a
// return value) |rval| for convenient access.

// Record/replay a function that returns a value and has a particular ABI.
#define RecordReplayFunctionABI(aName, aReturnType, aABI, ...)          \
  AutoRecordReplayFunction<aReturnType> rrf(CallEvent_ ##aName, #aName); \
  if (!rrf.mThread) {                                                   \
    return OriginalCallABI(aName, aReturnType, aABI, ##__VA_ARGS__);    \
  }                                                                     \
  if (IsRecording()) {                                                  \
    rrf.mRval = OriginalCallABI(aName, aReturnType, aABI, ##__VA_ARGS__); \
  }                                                                     \
  rrf.StartRecordReplay();                                              \
  Stream& events = rrf.mThread->Events();                               \
  (void) events;                                                        \
  aReturnType& rval = rrf.mRval

// Record/replay a function that returns a value and has the default ABI.
#define RecordReplayFunction(aName, aReturnType, ...)                   \
  RecordReplayFunctionABI(aName, aReturnType, DEFAULTABI, ##__VA_ARGS__)

// Record/replay a function that has no return value and has a particular ABI.
#define RecordReplayFunctionVoidABI(aName, aABI, ...)                   \
  AutoRecordReplayFunctionVoid rrf(CallEvent_ ##aName, #aName);         \
  if (!rrf.mThread) {                                                   \
    OriginalCallABI(aName, void, aABI, ##__VA_ARGS__);                  \
    return;                                                             \
  }                                                                     \
  if (IsRecording()) {                                                  \
    OriginalCallABI(aName, void, aABI, ##__VA_ARGS__);                  \
  }                                                                     \
  rrf.StartRecordReplay();                                              \
  Stream& events = rrf.mThread->Events();                               \
  (void) events

// Record/replay a function that has no return value and has the default ABI.
#define RecordReplayFunctionVoid(aName, ...)                    \
  RecordReplayFunctionVoidABI(aName, DEFAULTABI, ##__VA_ARGS__)

// The following macros are used for functions that do not record an error and
// take or return values of specified types.
//
// aAT == aArgumentType
// aRT == aReturnType

#define RRFunctionTypes0(aName, aRT)                             \
  static aRT DEFAULTABI                                          \
  RR_ ##aName ()                                                 \
  {                                                              \
    RecordReplayFunction(aName, aRT);                            \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes1(aName, aRT, aAT0)                       \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0)                                          \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0);                        \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes2(aName, aRT, aAT0, aAT1)                 \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1)                                 \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1);                    \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes3(aName, aRT, aAT0, aAT1, aAT2)           \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2)                        \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2);                \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes4(aName, aRT, aAT0, aAT1, aAT2, aAT3)     \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3)               \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3);            \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes5(aName, aRT, aAT0, aAT1, aAT2, aAT3,     \
                         aAT4)                                   \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4)      \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4);        \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes6(aName, aRT, aAT0, aAT1, aAT2, aAT3,     \
                         aAT4, aAT5)                             \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4,      \
               aAT5 a5)                                          \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4, a5);    \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes7(aName, aRT, aAT0, aAT1, aAT2, aAT3,     \
                         aAT4, aAT5, aAT6)                       \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4,      \
               aAT5 a5, aAT6 a6)                                 \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4, a5, a6); \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes8(aName, aRT, aAT0, aAT1, aAT2, aAT3,     \
                         aAT4, aAT5, aAT6, aAT7)                 \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4,      \
               aAT5 a5, aAT6 a6, aAT7 a7)                        \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4, a5, a6, a7); \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes9(aName, aRT, aAT0, aAT1, aAT2, aAT3,     \
                         aAT4, aAT5, aAT6, aAT7, aAT8)           \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4,      \
               aAT5 a5, aAT6 a6, aAT7 a7, aAT8 a8)               \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4, a5, a6, a7, a8); \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypes10(aName, aRT, aAT0, aAT1, aAT2, aAT3,    \
                          aAT4, aAT5, aAT6, aAT7, aAT8, aAT9)    \
  static aRT DEFAULTABI                                          \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4,      \
               aAT5 a5, aAT6 a6, aAT7 a7, aAT8 a8, aAT9 a9)      \
  {                                                              \
    RecordReplayFunction(aName, aRT, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9); \
    events.RecordOrReplayValue(&rval);                           \
    return rval;                                                 \
  }

#define RRFunctionTypesVoid1(aName, aAT0)                        \
  static void DEFAULTABI                                         \
  RR_ ##aName (aAT0 a0)                                          \
  {                                                              \
    RecordReplayFunctionVoid(aName, a0);                         \
  }

#define RRFunctionTypesVoid2(aName, aAT0, aAT1)                  \
  static void DEFAULTABI                                         \
  RR_ ##aName (aAT0 a0, aAT1 a1)                                 \
  {                                                              \
    RecordReplayFunctionVoid(aName, a0, a1);                     \
  }

#define RRFunctionTypesVoid3(aName, aAT0, aAT1, aAT2)            \
  static void DEFAULTABI                                         \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2)                        \
  {                                                              \
    RecordReplayFunctionVoid(aName, a0, a1, a2);                 \
  }

#define RRFunctionTypesVoid4(aName, aAT0, aAT1, aAT2, aAT3)      \
  static void DEFAULTABI                                         \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3)               \
  {                                                              \
    RecordReplayFunctionVoid(aName, a0, a1, a2, a3);             \
  }

#define RRFunctionTypesVoid5(aName, aAT0, aAT1, aAT2, aAT3, aAT4) \
  static void DEFAULTABI                                         \
  RR_ ##aName (aAT0 a0, aAT1 a1, aAT2 a2, aAT3 a3, aAT4 a4)      \
  {                                                              \
    RecordReplayFunctionVoid(aName, a0, a1, a2, a3, a4);         \
  }

// The following macros are used for functions that take and return scalar
// values (not a struct or a floating point) and do not record an error
// anywhere.

#define RRFunction0(aName) \
  RRFunctionTypes0(aName, size_t)

#define RRFunction1(aName) \
  RRFunctionTypes1(aName, size_t, size_t)

#define RRFunction2(aName) \
  RRFunctionTypes2(aName, size_t, size_t, size_t)

#define RRFunction3(aName) \
  RRFunctionTypes3(aName, size_t, size_t, size_t, size_t)

#define RRFunction4(aName) \
  RRFunctionTypes4(aName, size_t, size_t, size_t, size_t, size_t)

#define RRFunction5(aName) \
  RRFunctionTypes5(aName, size_t, size_t, size_t, size_t, size_t, size_t)

#define RRFunction6(aName) \
  RRFunctionTypes6(aName, size_t, size_t, size_t, size_t, size_t, size_t, size_t)

#define RRFunction7(aName) \
  RRFunctionTypes7(aName, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t)

#define RRFunction8(aName) \
  RRFunctionTypes8(aName, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, \
                   size_t)

#define RRFunction9(aName) \
  RRFunctionTypes9(aName, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, \
                   size_t, size_t)

#define RRFunction10(aName) \
  RRFunctionTypes10(aName, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, \
                    size_t, size_t, size_t)

// The following macros are used for functions that take scalar arguments and
// do not return a value or record an error anywhere.

#define RRFunctionVoid0(aName)                                   \
  static void DEFAULTABI                                         \
  RR_ ##aName ()                                                 \
  {                                                              \
    RecordReplayFunctionVoid(aName);                             \
  }

#define RRFunctionVoid1(aName) \
  RRFunctionTypesVoid1(aName, size_t)

#define RRFunctionVoid2(aName) \
  RRFunctionTypesVoid2(aName, size_t, size_t)

#define RRFunctionVoid3(aName) \
  RRFunctionTypesVoid3(aName, size_t, size_t, size_t)

#define RRFunctionVoid4(aName) \
  RRFunctionTypesVoid4(aName, size_t, size_t, size_t, size_t)

#define RRFunctionVoid5(aName) \
  RRFunctionTypesVoid5(aName, size_t, size_t, size_t, size_t, size_t)

// The following macros are used for functions that return a signed integer
// value and record an error if the return value is negative.

#define RRFunctionNegError0(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName ()                                                 \
  {                                                              \
    RecordReplayFunction(aName, ssize_t);                        \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError1(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0)                                        \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0);                    \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError2(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0, size_t a1)                             \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0, a1);                \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError3(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0, size_t a1, size_t a2)                  \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0, a1, a2);            \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError4(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3)       \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0, a1, a2, a3);        \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError5(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4)                                        \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0, a1, a2, a3, a4);    \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

#define RRFunctionNegError6(aName)                               \
  static ssize_t DEFAULTABI                                      \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4, size_t a5)                             \
  {                                                              \
    RecordReplayFunction(aName, ssize_t, a0, a1, a2, a3, a4, a5); \
    RecordOrReplayHadErrorNegative(rrf);                         \
    return rval;                                                 \
  }

// The following macros are used for functions that return an integer
// value and record an error if the return value is zero.

#define RRFunctionZeroError0(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName ()                                                 \
  {                                                              \
    RecordReplayFunction(aName, size_t);                         \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError1(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0)                                        \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0);                     \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroErrorABI2(aName, aABI)                     \
  static size_t aABI                                             \
  RR_ ##aName (size_t a0, size_t a1)                             \
  {                                                              \
    RecordReplayFunctionABI(aName, size_t, aABI, a0, a1);        \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }
#define RRFunctionZeroError2(aName) RRFunctionZeroErrorABI2(aName, DEFAULTABI)

#define RRFunctionZeroError3(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2)                  \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2);             \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError4(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3)       \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2, a3);         \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError5(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4)                                        \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2, a3, a4);     \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError6(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4, size_t a5)                             \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2, a3, a4, a5); \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError7(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4, size_t a5, size_t a6)                  \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2, a3, a4, a5, a6); \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

#define RRFunctionZeroError8(aName)                              \
  static size_t __stdcall                                        \
  RR_ ##aName (size_t a0, size_t a1, size_t a2, size_t a3,       \
               size_t a4, size_t a5, size_t a6, size_t a7)       \
  {                                                              \
    RecordReplayFunction(aName, size_t, a0, a1, a2, a3, a4, a5, a6, a7); \
    RecordOrReplayHadErrorZero(rrf);                             \
    return rval;                                                 \
  }

// Recording template for functions which are used for inter-thread
// synchronization and must be replayed in the original order they executed in.
#define RecordReplayOrderedFunction(aName, aReturnType, aFailureRval, aFormals, ...) \
  static aReturnType DEFAULTABI                                  \
  RR_ ## aName aFormals                                          \
  {                                                              \
    BeginOrderedEvent(); /* This is a noop if !mThread */        \
    RecordReplayFunction(aName, aReturnType, __VA_ARGS__);       \
    EndOrderedEvent();                                           \
    events.RecordOrReplayValue(&rval);                           \
    if (rval == aFailureRval) {                                  \
      events.RecordOrReplayValue(&rrf.mError);                   \
    }                                                            \
    return rval;                                                 \
  }

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

struct CallbackWrapperData
{
  void* mFunction;
  void* mData;

  template <typename FunctionType>
  CallbackWrapperData(FunctionType aFunction, void* aData)
    : mFunction(BitwiseCast<void*>(aFunction)), mData(aData)
  {}
};

// This class should not be used directly, but rather through the macro below.
template <typename FunctionType>
struct AutoRecordReplayCallback
{
  FunctionType mFunction;

  AutoRecordReplayCallback(void** aDataArgument, size_t aCallbackId)
    : mFunction(nullptr)
  {
    MOZ_ASSERT(IsRecordingOrReplaying());
    if (IsRecording()) {
      CallbackWrapperData* wrapperData = (CallbackWrapperData*) *aDataArgument;
      mFunction = (FunctionType) wrapperData->mFunction;
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
#define RecordReplayCallback(aFunctionType, aDataArgument)              \
  AutoRecordReplayCallback<aFunctionType> rrc(aDataArgument, CallbackEvent_ ##aFunctionType)

///////////////////////////////////////////////////////////////////////////////
// Redirection Helpers
///////////////////////////////////////////////////////////////////////////////

// Read/write a success code (where zero is failure) and errno value on failure.
template <typename T>
static inline bool
RecordOrReplayHadErrorZero(AutoRecordReplayFunction<T>& aRrf)
{
  aRrf.mThread->Events().RecordOrReplayValue(&aRrf.mRval);
  if (aRrf.mRval == 0) {
    aRrf.mThread->Events().RecordOrReplayValue(&aRrf.mError);
    return true;
  }
  return false;
}

// Read/write a success code (where negative values are failure) and errno value on failure.
template <typename T>
static inline bool
RecordOrReplayHadErrorNegative(AutoRecordReplayFunction<T>& aRrf)
{
  aRrf.mThread->Events().RecordOrReplayValue(&aRrf.mRval);
  if (aRrf.mRval < 0) {
    aRrf.mThread->Events().RecordOrReplayValue(&aRrf.mError);
    return true;
  }
  return false;
}

extern Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gMemoryLeakBytes;

// For allocating memory in redirections that will never be reclaimed. This is
// done for simplicity. If the amount of leaked memory from redirected calls
// grows too large then steps can be taken to more closely emulate the library
// behavior.
template <typename T>
static inline T*
NewLeakyArray(size_t aSize)
{
  gMemoryLeakBytes += aSize * sizeof(T);
  return new T[aSize];
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
void*
BindFunctionArgument(void* aFunction, void* aArgument, size_t aArgumentPosition,
                     Assembler& aAssembler);

} // recordreplay
} // mozilla

#endif // mozilla_recordreplay_ProcessRedirect_h
