/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ExternalCall_h
#define mozilla_recordreplay_ExternalCall_h

#include "BufferStream.h"
#include "ProcessRedirect.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace recordreplay {

// External Calls Overview
//
// With few exceptions, replaying processes do not interact with the underlying
// system or call the actual versions of redirected system library functions.
// This is problematic after diverging from the recording, as then the diverged
// thread cannot interact with its recording either.
//
// External calls are used in a replaying process after diverging from the
// recording to perform calls in another process instead. We call this the
// external process; currently it is always the middleman, though this will
// change soon.
//
// External call state is managed so that call results can be reused when
// possible, to minimize traffic between processes and improve efficiency.
// Conceptually, the result of performing a system call is entirely determined
// by its inputs: scalar values and the results of other system calls (and the
// version of the underlying system, which we ignore for now). This information
// uniquely identifies the call, and we can form a directed graph out of these
// calls by connecting them to the other calls they depend on.
//
// Each root replaying process maintains a portion of this graph. As the
// recording is replayed by other forked processes, nodes are added to the graph
// by studying the output of calls that appear in the recording itself. When
// a replaying process wants to make a system call that does not appear in the
// graph, the inputs for that call and the transitive closure of the calls it
// depends on is sent to another process running on the target platform. That
// process executes the call, saves the output and sends it back for adding to
// the graph. Other ways of updating the graph could be added in the future.

// Inputs and outputs to external calls are handled in a reusable fashion by
// adding a ExternalCall hook to the system call's redirection. This hook is
// called in one of the following phases.
enum class ExternalCallPhase {
  // When replaying, a call which can be put in the external call graph is being
  // made. This can happen either before or after diverging from the recording,
  // and saves all inputs to the call which are sufficient to uniquely identify
  // it in the graph.
  SaveInput,

  // In the external process, the inputs saved earlier are being restored in
  // preparation for executing the call.
  RestoreInput,

  // Save any outputs produced by a call. This can happen either in the
  // replaying process (using outputs saved in the recording) or in the external
  // process (using outputs just produced). After saving the outputs to the
  // call, it can be placed in the external call graph and be used to resolve
  // external calls which a diverged replaying process is making. Returned
  // register values are automatically saved.
  SaveOutput,

  // In the replaying process, restore any outputs associated with an external
  // call. This is only called after diverging from the recording, and allows
  // diverged execution to continue afterwards.
  RestoreOutput,
};

// Global identifier for an external call. The result of an external call is
// determined by its inputs, and its ID is a hash of those inputs. If there are
// hash collisions between different inputs then two different calls will have
// the same ID, and things will break (subtly or not). Valid IDs are non-zero.
typedef uintptr_t ExternalCallId;

// Storage for the returned registers of a call which are automatically saved.
struct CallReturnRegisters {
  size_t rval0, rval1;
  double floatrval0, floatrval1;

  void CopyFrom(CallArguments* aArguments);
  void CopyTo(CallArguments* aArguments);
};

struct ExternalCall {
  // ID for this call.
  ExternalCallId mId = 0;

  // ID of the redirection being invoked.
  size_t mCallId = 0;

  // All call inputs. Written in SaveInput, read in RestoreInput.
  InfallibleVector<char> mInput;

  // If non-zero, only the input before this extent is used to characterize this
  // call and determine if it is the same as another external call.
  size_t mExcludeInput = 0;

  // Calls which this depends on, written in SaveInput and read in RestoreInput.
  // Any calls referenced in mInput will also be here.
  InfallibleVector<ExternalCallId> mDependentCalls;

  // All call outputs. Written in SaveOutput, read in RestoreOutput.
  InfallibleVector<char> mOutput;

  // Values of any returned registers after the call.
  CallReturnRegisters mReturnRegisters;

  // Any system value produced by this call. In the external process this is
  // the actual system value, while in the replaying process this is the value
  // used during execution to represent the call's result.
  Maybe<const void*> mValue;

  void EncodeInput(BufferStream& aStream) const;
  void DecodeInput(BufferStream& aStream);

  void EncodeOutput(BufferStream& aStream) const;
  void DecodeOutput(BufferStream& aStream);

  void ComputeId() {
    MOZ_RELEASE_ASSERT(!mId);
    size_t extent = mExcludeInput ? mExcludeInput : mInput.length();
    mId = HashGeneric(mCallId, HashBytes(mInput.begin(), extent));
    if (!mId) {
      mId = 1;
    }
  }
};

// Information needed to process one of the phases of an external call.
struct ExternalCallContext {
  // Call being operated on.
  ExternalCall* mCall;

  // Complete arguments and return value information for the call.
  CallArguments* mArguments;

  // Current processing phase.
  ExternalCallPhase mPhase;

  // During the SaveInput phase, whether capturing input data has failed.
  // In such cases the call cannot be placed in the external call graph and,
  // if the thread has diverged from the recording, an unhandled divergence
  // will occur.
  bool mFailed = false;

  // This can be set in the RestoreInput phase to avoid executing the call
  // in the external process.
  bool mSkipExecuting = false;

  // Streams of data that can be accessed during the various phases. Streams
  // need to be read or written from at the same points in the phases which use
  // them, so that callbacks operating on these streams can be composed without
  // issues.

  // Inputs are written during SaveInput, and read during RestoreInput.
  Maybe<BufferStream> mInputStream;

  // Outputs are written during SaveOutput, and read during RestoreOutput.
  Maybe<BufferStream> mOutputStream;

  // If we are running the SaveOutput phase in an external process, a list of
  // callbacks which will release all system resources created by by the call.
  typedef InfallibleVector<std::function<void()>> ReleaseCallbackVector;
  ReleaseCallbackVector* mReleaseCallbacks = nullptr;

  ExternalCallContext(ExternalCall* aCall, CallArguments* aArguments,
                      ExternalCallPhase aPhase)
      : mCall(aCall),
        mArguments(aArguments),
        mPhase(aPhase) {
    switch (mPhase) {
      case ExternalCallPhase::SaveInput:
        mInputStream.emplace(&mCall->mInput);
        break;
      case ExternalCallPhase::RestoreInput:
        mInputStream.emplace(mCall->mInput.begin(), mCall->mInput.length());
        break;
      case ExternalCallPhase::SaveOutput:
        mCall->mReturnRegisters.CopyFrom(aArguments);
        mOutputStream.emplace(&mCall->mOutput);
        break;
      case ExternalCallPhase::RestoreOutput:
        mCall->mReturnRegisters.CopyTo(aArguments);
        mOutputStream.emplace(mCall->mOutput.begin(), mCall->mOutput.length());
        break;
    }
  }

  void MarkAsFailed() {
    MOZ_RELEASE_ASSERT(mPhase == ExternalCallPhase::SaveInput);
    mFailed = true;
  }

  void WriteInputBytes(const void* aBuffer, size_t aSize) {
    MOZ_RELEASE_ASSERT(mPhase == ExternalCallPhase::SaveInput);
    mInputStream.ref().WriteBytes(aBuffer, aSize);
  }

  void WriteInputScalar(size_t aValue) {
    MOZ_RELEASE_ASSERT(mPhase == ExternalCallPhase::SaveInput);
    mInputStream.ref().WriteScalar(aValue);
  }

  void ReadInputBytes(void* aBuffer, size_t aSize) {
    MOZ_RELEASE_ASSERT(mPhase == ExternalCallPhase::RestoreInput);
    mInputStream.ref().ReadBytes(aBuffer, aSize);
  }

  size_t ReadInputScalar() {
    MOZ_RELEASE_ASSERT(mPhase == ExternalCallPhase::RestoreInput);
    return mInputStream.ref().ReadScalar();
  }

  bool AccessInput() { return mInputStream.isSome(); }

  void ReadOrWriteInputBytes(void* aBuffer, size_t aSize, bool aExcludeInput = false) {
    switch (mPhase) {
      case ExternalCallPhase::SaveInput:
        // Only one buffer can be excluded, and it has to be the last input to
        // the external call.
        MOZ_RELEASE_ASSERT(!mCall->mExcludeInput);
        if (aExcludeInput) {
          mCall->mExcludeInput = mCall->mInput.length();
        }
        WriteInputBytes(aBuffer, aSize);
        break;
      case ExternalCallPhase::RestoreInput:
        ReadInputBytes(aBuffer, aSize);
        break;
      default:
        MOZ_CRASH();
    }
  }

  void ReadOrWriteInputBuffer(void** aBufferPtr, size_t aSize,
                              bool aIncludeContents = true) {
    switch (mPhase) {
      case ExternalCallPhase::SaveInput:
        if (aIncludeContents) {
          WriteInputBytes(*aBufferPtr, aSize);
        }
        break;
      case ExternalCallPhase::RestoreInput:
        *aBufferPtr = AllocateBytes(aSize);
        if (aIncludeContents) {
          ReadInputBytes(*aBufferPtr, aSize);
        }
        break;
      default:
        MOZ_CRASH();
    }
  }

  bool AccessOutput() { return mOutputStream.isSome(); }

  void ReadOrWriteOutputBytes(void* aBuffer, size_t aSize) {
    switch (mPhase) {
      case ExternalCallPhase::SaveOutput:
        mOutputStream.ref().WriteBytes(aBuffer, aSize);
        break;
      case ExternalCallPhase::RestoreOutput:
        mOutputStream.ref().ReadBytes(aBuffer, aSize);
        break;
      default:
        MOZ_CRASH();
    }
  }

  void ReadOrWriteOutputBuffer(void** aBuffer, size_t aSize) {
    if (AccessInput()) {
      bool isNull = *aBuffer == nullptr;
      ReadOrWriteInputBytes(&isNull, sizeof(isNull));
      if (isNull) {
        *aBuffer = nullptr;
      } else if (mPhase == ExternalCallPhase::RestoreInput) {
        *aBuffer = AllocateBytes(aSize);
      }
    }

    if (AccessOutput() && *aBuffer) {
      ReadOrWriteOutputBytes(*aBuffer, aSize);
    }
  }

  // Allocate some memory associated with the call, which will be released in
  // the external process after fully processing a call, and will never be
  // released in the replaying process.
  void* AllocateBytes(size_t aSize);
};

// Notify the system about a call to a redirection with an external call hook.
// aDiverged is set if the current thread has diverged from the recording and
// any outputs for the call must be filled in; otherwise, they already have
// been filled in using data from the recording. Returns false if the call was
// unable to be processed.
bool OnExternalCall(size_t aCallId, CallArguments* aArguments,
                    bool aDiverged);

// In the external process, perform one or more calls encoded in aInputData
// and encode the output of the final call in aOutputData.
void ProcessExternalCall(const char* aInputData, size_t aInputSize,
                         InfallibleVector<char>* aOutputData);

// In a replaying process, flush all new external call found in the recording
// since the last flush to the root replaying process.
void FlushExternalCalls();

// In a root replaying process, remember the output from an external call.
void AddExternalCallOutput(ExternalCallId aId, const char* aOutput,
                           size_t aOutputSize);

// In a root replaying process, fetch the output from an external call if known.
bool HasExternalCallOutput(ExternalCallId aId, InfallibleVector<char>* aOutput);

///////////////////////////////////////////////////////////////////////////////
// External Call Helpers
///////////////////////////////////////////////////////////////////////////////

// Capture a scalar argument.
template <size_t Arg>
static inline void EX_ScalarArg(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto& arg = aCx.mArguments->Arg<Arg, size_t>();
    aCx.ReadOrWriteInputBytes(&arg, sizeof(arg));
  }
}

// Capture a floating point argument.
template <size_t Arg>
static inline void EX_FloatArg(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto& arg = aCx.mArguments->FloatArg<Arg>();
    aCx.ReadOrWriteInputBytes(&arg, sizeof(arg));
  }
}

// Capture an input buffer at BufferArg with element count at CountArg.
// If IncludeContents is not set, the buffer's contents are not captured,
// but the buffer's pointer will be allocated with the correct size when
// restoring input.
template <size_t BufferArg, size_t CountArg, typename ElemType = char,
          bool IncludeContents = true>
static inline void EX_Buffer(ExternalCallContext& aCx) {
  EX_ScalarArg<CountArg>(aCx);

  if (aCx.AccessInput()) {
    auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
    auto byteSize = aCx.mArguments->Arg<CountArg, size_t>() * sizeof(ElemType);
    aCx.ReadOrWriteInputBuffer(&buffer, byteSize, IncludeContents);
  }
}

// Capture the contents of an optional input parameter.
template <size_t BufferArg, typename Type>
static inline void EX_InParam(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto& param = aCx.mArguments->Arg<BufferArg, void*>();
    bool hasParam = !!param;
    aCx.ReadOrWriteInputBytes(&hasParam, sizeof(hasParam));
    if (hasParam) {
      aCx.ReadOrWriteInputBuffer(&param, sizeof(Type));
    } else {
      param = nullptr;
    }
  }
}

// Capture a C string argument.
template <size_t StringArg>
static inline void EX_CString(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto& buffer = aCx.mArguments->Arg<StringArg, char*>();
    size_t len = (aCx.mPhase == ExternalCallPhase::SaveInput)
                     ? strlen(buffer) + 1
                     : 0;
    aCx.ReadOrWriteInputBytes(&len, sizeof(len));
    aCx.ReadOrWriteInputBuffer((void**)&buffer, len);
  }
}

// Capture the data written to an output buffer at BufferArg with element count
// at CountArg.
template <size_t BufferArg, size_t CountArg, typename ElemType>
static inline void EX_WriteBuffer(ExternalCallContext& aCx) {
  EX_ScalarArg<CountArg>(aCx);

  auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
  auto count = aCx.mArguments->Arg<CountArg, size_t>();
  aCx.ReadOrWriteOutputBuffer(&buffer, count * sizeof(ElemType));
}

// Capture the data written to an out parameter.
template <size_t BufferArg, typename Type>
static inline void EX_OutParam(ExternalCallContext& aCx) {
  auto& buffer = aCx.mArguments->Arg<BufferArg, void*>();
  aCx.ReadOrWriteOutputBuffer(&buffer, sizeof(Type));
}

// Capture return values that are too large for register storage.
template <typename Type>
static inline void EX_OversizeRval(ExternalCallContext& aCx) {
  EX_OutParam<0, Type>(aCx);
}

// Capture a byte count of stack argument data.
template <size_t ByteSize>
static inline void EX_StackArgumentData(ExternalCallContext& aCx) {
  if (aCx.AccessInput()) {
    auto stack = aCx.mArguments->StackAddress<0>();
    aCx.ReadOrWriteInputBytes(stack, ByteSize);
  }
}

// Avoid calling a function in the external process.
static inline void EX_SkipExecuting(ExternalCallContext& aCx) {
  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    aCx.mSkipExecuting = true;
  }
}

static inline void EX_NoOp(ExternalCallContext& aCx) {}

template <ExternalCallFn Fn0, ExternalCallFn Fn1,
          ExternalCallFn Fn2 = EX_NoOp, ExternalCallFn Fn3 = EX_NoOp,
          ExternalCallFn Fn4 = EX_NoOp, ExternalCallFn Fn5 = EX_NoOp>
static inline void EX_Compose(ExternalCallContext& aCx) {
  Fn0(aCx);
  Fn1(aCx);
  Fn2(aCx);
  Fn3(aCx);
  Fn4(aCx);
  Fn5(aCx);
}

// Helper for capturing inputs that are produced by other external calls.
// Returns false in the SaveInput phase if the input system value could not
// be found.
bool EX_SystemInput(ExternalCallContext& aCx, const void** aThingPtr);

// Helper for capturing output system values that might be consumed by other
// external calls.
void EX_SystemOutput(ExternalCallContext& aCx, const void** aOutput,
                     bool aUpdating = false);

void InitializeExternalCalls();

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ExternalCall_h
