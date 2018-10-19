/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_MiddlemanCall_h
#define mozilla_recordreplay_MiddlemanCall_h

#include "BufferStream.h"
#include "ProcessRedirect.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace recordreplay {

// Middleman Calls Overview
//
// With few exceptions, replaying processes do not interact with the underlying
// system or call the actual versions of redirected system library functions.
// This is problematic after diverging from the recording, as then the diverged
// thread cannot interact with its recording either.
//
// Middleman calls are used in a replaying process after diverging from the
// recording to perform calls in the middleman process instead. Inputs are
// gathered and serialized in the replaying process, then sent to the middleman
// process. The middleman calls the function, and its outputs are serialized
// for reading by the replaying process.
//
// Calls that might need to be sent to the middleman are processed in phases,
// per the MiddlemanCallPhase enum below. The timeline of a middleman call is
// as follows:
//
// - Any redirection with a middleman call hook can potentially be sent to the
//   middleman. In a replaying process, whenever such a call is encountered,
//   the hook is invoked in the ReplayPreface phase to capture any input data
//   that must be examined at the time of the call itself.
//
// - If the thread has not diverged from the recording, the call is remembered
//   but no further action is necessary yet.
//
// - If the thread has diverged from the recording, the call needs to go
//   through the remaining phases. The ReplayInput phase captures any
//   additional inputs to the call, potentially including values produced by
//   other middleman calls.
//
// - The transitive closure of these call dependencies is produced, and all
//   calls found go through the ReplayInput phase. The resulting data is sent
//   to the middleman process, which goes through the MiddlemanInput phase
//   to decode those inputs.
//
// - The middleman performs each of the calls it has been given, and their
//   outputs are encoded in the MiddlemanOutput phase. These outputs are sent
//   to the replaying process in a response and decoded in the ReplayOutput
//   phase, which can then resume execution.
//
// - The replaying process holds onto information about calls it has sent until
//   it rewinds to a point before it diverged from the recording. This rewind
//   will --- without any special action required --- wipe out information on
//   all calls sent to the middleman, and retain any data gathered in the
//   ReplayPreface phase for calls that were made prior to the rewind target.
//
// - Information about calls and all resources held are retained in the
//   middleman process are retained until a replaying process asks for them to
//   be reset, which happens any time the replaying process first diverges from
//   the recording. The MiddlemanRelease phase is used to release any system
//   resources held.

// Ways of processing calls that can be sent to the middleman.
enum class MiddlemanCallPhase
{
  // When replaying, a call is being performed that might need to be sent to
  // the middleman later.
  ReplayPreface,

  // A call for which inputs have been gathered is now being sent to the
  // middleman. This is separate from ReplayPreface because capturing inputs
  // might need to dereference pointers that could be bogus values originating
  // from the recording. Waiting to dereference these pointers until we know
  // the call needs to be sent to the middleman avoids needing to understand
  // the inputs to all call sites of general purpose redirections such as
  // CFArrayCreate.
  ReplayInput,

  // In the middleman process, a call from the replaying process is being
  // performed.
  MiddlemanInput,

  // In the middleman process, a call from the replaying process was just
  // performed, and its outputs need to be saved.
  MiddlemanOutput,

  // Back in the replaying process, the outputs from a call have been received
  // from the middleman.
  ReplayOutput,

  // In the middleman process, release any system resources held after this
  // call.
  MiddlemanRelease,
};

struct MiddlemanCall
{
  // Unique ID for this call.
  size_t mId;

  // ID of the redirection being invoked.
  size_t mCallId;

  // All register arguments and return values are preserved when sending the
  // call back and forth between processes.
  CallRegisterArguments mArguments;

  // Written in ReplayPrefaceInput, read in ReplayInput and MiddlemanInput.
  InfallibleVector<char> mPreface;

  // Written in ReplayInput, read in MiddlemanInput.
  InfallibleVector<char> mInput;

  // Written in MiddlemanOutput, read in ReplayOutput.
  InfallibleVector<char> mOutput;

  // In a replaying process, whether this call has been sent to the middleman.
  bool mSent;

  // In a replaying process, any value associated with this call that was
  // included in the recording, when the call was made before diverging from
  // the recording.
  Maybe<const void*> mRecordingValue;

  // In a replaying or middleman process, any value associated with this call
  // that was produced by the middleman itself.
  Maybe<const void*> mMiddlemanValue;

  MiddlemanCall()
    : mId(0), mCallId(0), mSent(false)
  {}

  void EncodeInput(BufferStream& aStream) const;
  void DecodeInput(BufferStream& aStream);

  void EncodeOutput(BufferStream& aStream) const;
  void DecodeOutput(BufferStream& aStream);

  void SetRecordingValue(const void* aValue) {
    MOZ_RELEASE_ASSERT(mRecordingValue.isNothing());
    mRecordingValue.emplace(aValue);
  }

  void SetMiddlemanValue(const void* aValue) {
    MOZ_RELEASE_ASSERT(mMiddlemanValue.isNothing());
    mMiddlemanValue.emplace(aValue);
  }
};

// Information needed to process one of the phases of a middleman call,
// in either the replaying or middleman process.
struct MiddlemanCallContext
{
  // Call being operated on.
  MiddlemanCall* mCall;

  // Complete arguments and return value information for the call.
  CallArguments* mArguments;

  // Current processing phase.
  MiddlemanCallPhase mPhase;

  // During the ReplayPreface or ReplayInput phases, whether capturing input
  // data has failed. In such cases the call cannot be sent to the middleman
  // and, if the thread has diverged from the recording, an unhandled
  // divergence and associated rewind will occur.
  bool mFailed;

  // During the ReplayInput phase, this can be used to fill in any middleman
  // calls whose output the current one depends on.
  InfallibleVector<MiddlemanCall*>* mDependentCalls;

  // Streams of data that can be accessed during the various phases. Streams
  // need to be read or written from at the same points in the phases which use
  // them, so that callbacks operating on these streams can be composed without
  // issues.

  // The preface is written during ReplayPreface, and read during both
  // ReplayInput and MiddlemanInput.
  Maybe<BufferStream> mPrefaceStream;

  // Inputs are written during ReplayInput, and read during MiddlemanInput.
  Maybe<BufferStream> mInputStream;

  // Outputs are written during MiddlemanOutput, and read during ReplayOutput.
  Maybe<BufferStream> mOutputStream;

  // During the ReplayOutput phase, this is set if the call was made sometime
  // in the past and pointers referred to in the arguments may no longer be
  // valid.
  bool mReplayOutputIsOld;

  MiddlemanCallContext(MiddlemanCall* aCall, CallArguments* aArguments, MiddlemanCallPhase aPhase)
    : mCall(aCall), mArguments(aArguments), mPhase(aPhase),
      mFailed(false), mDependentCalls(nullptr), mReplayOutputIsOld(false)
  {
    switch (mPhase) {
    case MiddlemanCallPhase::ReplayPreface:
      mPrefaceStream.emplace(&mCall->mPreface);
      break;
    case MiddlemanCallPhase::ReplayInput:
      mPrefaceStream.emplace(mCall->mPreface.begin(), mCall->mPreface.length());
      mInputStream.emplace(&mCall->mInput);
      break;
    case MiddlemanCallPhase::MiddlemanInput:
      mPrefaceStream.emplace(mCall->mPreface.begin(), mCall->mPreface.length());
      mInputStream.emplace(mCall->mInput.begin(), mCall->mInput.length());
      break;
    case MiddlemanCallPhase::MiddlemanOutput:
      mOutputStream.emplace(&mCall->mOutput);
      break;
    case MiddlemanCallPhase::ReplayOutput:
      mOutputStream.emplace(mCall->mOutput.begin(), mCall->mOutput.length());
      break;
    case MiddlemanCallPhase::MiddlemanRelease:
      break;
    }
  }

  void MarkAsFailed() {
    MOZ_RELEASE_ASSERT(mPhase == MiddlemanCallPhase::ReplayPreface ||
                       mPhase == MiddlemanCallPhase::ReplayInput);
    mFailed = true;
  }

  void WriteInputBytes(const void* aBuffer, size_t aSize) {
    MOZ_RELEASE_ASSERT(mPhase == MiddlemanCallPhase::ReplayInput);
    mInputStream.ref().WriteBytes(aBuffer, aSize);
  }

  void WriteInputScalar(size_t aValue) {
    MOZ_RELEASE_ASSERT(mPhase == MiddlemanCallPhase::ReplayInput);
    mInputStream.ref().WriteScalar(aValue);
  }

  void ReadInputBytes(void* aBuffer, size_t aSize) {
    MOZ_RELEASE_ASSERT(mPhase == MiddlemanCallPhase::MiddlemanInput);
    mInputStream.ref().ReadBytes(aBuffer, aSize);
  }

  size_t ReadInputScalar() {
    MOZ_RELEASE_ASSERT(mPhase == MiddlemanCallPhase::MiddlemanInput);
    return mInputStream.ref().ReadScalar();
  }

  bool AccessInput() {
    return mInputStream.isSome();
  }

  void ReadOrWriteInputBytes(void* aBuffer, size_t aSize) {
    switch (mPhase) {
    case MiddlemanCallPhase::ReplayInput:
      WriteInputBytes(aBuffer, aSize);
      break;
    case MiddlemanCallPhase::MiddlemanInput:
      ReadInputBytes(aBuffer, aSize);
      break;
    default:
      MOZ_CRASH();
    }
  }

  bool AccessPreface() {
    return mPrefaceStream.isSome();
  }

  void ReadOrWritePrefaceBytes(void* aBuffer, size_t aSize) {
    switch (mPhase) {
    case MiddlemanCallPhase::ReplayPreface:
      mPrefaceStream.ref().WriteBytes(aBuffer, aSize);
      break;
    case MiddlemanCallPhase::ReplayInput:
    case MiddlemanCallPhase::MiddlemanInput:
      mPrefaceStream.ref().ReadBytes(aBuffer, aSize);
      break;
    default:
      MOZ_CRASH();
    }
  }

  void ReadOrWritePrefaceBuffer(void** aBufferPtr, size_t aSize) {
    switch (mPhase) {
    case MiddlemanCallPhase::ReplayPreface:
      mPrefaceStream.ref().WriteBytes(*aBufferPtr, aSize);
      break;
    case MiddlemanCallPhase::ReplayInput:
    case MiddlemanCallPhase::MiddlemanInput:
      *aBufferPtr = AllocateBytes(aSize);
      mPrefaceStream.ref().ReadBytes(*aBufferPtr, aSize);
      break;
    default:
      MOZ_CRASH();
    }
  }

  bool AccessOutput() {
    return mOutputStream.isSome();
  }

  void ReadOrWriteOutputBytes(void* aBuffer, size_t aSize) {
    switch (mPhase) {
    case MiddlemanCallPhase::MiddlemanOutput:
      mOutputStream.ref().WriteBytes(aBuffer, aSize);
      break;
    case MiddlemanCallPhase::ReplayOutput:
      mOutputStream.ref().ReadBytes(aBuffer, aSize);
      break;
    default:
      MOZ_CRASH();
    }
  }

  void ReadOrWriteOutputBuffer(void** aBuffer, size_t aSize) {
    if (*aBuffer) {
      if (mPhase == MiddlemanCallPhase::MiddlemanInput || mReplayOutputIsOld) {
        *aBuffer = AllocateBytes(aSize);
      }

      if (AccessOutput()) {
        ReadOrWriteOutputBytes(*aBuffer, aSize);
      }
    }
  }

  // Allocate some memory associated with the call, which will be released in
  // the replaying process on a rewind and in the middleman process when the
  // call state is reset.
  void* AllocateBytes(size_t aSize);
};

// Notify the system about a call to a redirection with a middleman call hook.
// aDiverged is set if the current thread has diverged from the recording and
// any outputs for the call must be filled in; otherwise, they already have
// been filled in using data from the recording. Returns false if the call was
// unable to be processed.
bool SendCallToMiddleman(size_t aCallId, CallArguments* aArguments, bool aDiverged);

// In the middleman process, perform one or more calls encoded in aInputData
// and encode their outputs to aOutputData.
void ProcessMiddlemanCall(const char* aInputData, size_t aInputSize,
                          InfallibleVector<char>* aOutputData);

// In the middleman process, reset all call state.
void ResetMiddlemanCalls();

///////////////////////////////////////////////////////////////////////////////
// Middleman Call Helpers
///////////////////////////////////////////////////////////////////////////////

// Capture the contents of an input buffer at BufferArg with element count at CountArg.
template <size_t BufferArg, size_t CountArg, typename ElemType>
static inline void
Middleman_Buffer(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
    auto byteSize = aCx.mArguments->Arg<CountArg, size_t>() * sizeof(ElemType);
    aCx.ReadOrWritePrefaceBuffer(&buffer, byteSize);
  }
}

// Capture the contents of a fixed size input buffer.
template <size_t BufferArg, size_t ByteSize>
static inline void
Middleman_BufferFixedSize(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
    if (buffer) {
      aCx.ReadOrWritePrefaceBuffer(&buffer, ByteSize);
    }
  }
}

// Capture a C string argument.
template <size_t StringArg>
static inline void
Middleman_CString(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto& buffer = aCx.mArguments->Arg<StringArg, char*>();
    size_t len = (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) ? strlen(buffer) + 1 : 0;
    aCx.ReadOrWritePrefaceBytes(&len, sizeof(len));
    aCx.ReadOrWritePrefaceBuffer((void**) &buffer, len);
  }
}

// Capture the data written to an output buffer at BufferArg with element count at CountArg.
template <size_t BufferArg, size_t CountArg, typename ElemType>
static inline void
Middleman_WriteBuffer(MiddlemanCallContext& aCx)
{
  auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
  auto count = aCx.mArguments->Arg<CountArg, size_t>();
  aCx.ReadOrWriteOutputBuffer(&buffer, count * sizeof(ElemType));
}

// Capture the data written to a fixed size output buffer.
template <size_t BufferArg, size_t ByteSize>
static inline void
Middleman_WriteBufferFixedSize(MiddlemanCallContext& aCx)
{
  auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
  aCx.ReadOrWriteOutputBuffer(&buffer, ByteSize);
}

// Capture return values that are too large for register storage.
template <size_t ByteSize>
static inline void
Middleman_OversizeRval(MiddlemanCallContext& aCx)
{
  Middleman_WriteBufferFixedSize<0, ByteSize>(aCx);
}

// Capture a byte count of stack argument data.
template <size_t ByteSize>
static inline void
Middleman_StackArgumentData(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto stack = aCx.mArguments->StackAddress<0>();
    aCx.ReadOrWritePrefaceBytes(stack, ByteSize);
  }
}

static inline void
Middleman_NoOp(MiddlemanCallContext& aCx)
{
}

template <MiddlemanCallFn Fn0,
          MiddlemanCallFn Fn1,
          MiddlemanCallFn Fn2 = Middleman_NoOp,
          MiddlemanCallFn Fn3 = Middleman_NoOp,
          MiddlemanCallFn Fn4 = Middleman_NoOp>
static inline void
Middleman_Compose(MiddlemanCallContext& aCx)
{
  Fn0(aCx);
  Fn1(aCx);
  Fn2(aCx);
  Fn3(aCx);
  Fn4(aCx);
}

// Helper for capturing inputs that are produced by other middleman calls.
// Returns false in the ReplayInput or MiddlemanInput phases if the input
// system value could not be found.
bool Middleman_SystemInput(MiddlemanCallContext& aCx, const void** aThingPtr);

// Helper for capturing output system values that might be consumed by other
// middleman calls.
void Middleman_SystemOutput(MiddlemanCallContext& aCx, const void** aOutput, bool aUpdating = false);

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_MiddlemanCall_h
