/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalCall.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Replaying and External Process State
///////////////////////////////////////////////////////////////////////////////

typedef std::unordered_map<ExternalCallId, ExternalCall*> CallsByIdMap;
typedef std::unordered_map<const void*, ExternalCallId> CallsByValueMap;

// State used for keeping track of external calls in either a replaying
// process or external process.
struct ExternalCallState {
  // In a replaying or external process, association between ExternalCallIds
  // and the associated ExternalCall, for each encountered call.
  CallsByIdMap mCallsById;

  // In a replaying or external process, association between values produced by
  // a external call and the call's ID. This is the inverse of each call's
  // mValue field, except that if multiple calls have produced the same value
  // this maps that value to the most recent one.
  CallsByValueMap mCallsByValue;

  // In an external process, any buffers allocated for performed calls.
  InfallibleVector<void*> mAllocatedBuffers;
};

// In a replaying process, all external call state. In an external process,
// state for the call currently being processed.
static ExternalCallState* gState;

// In a replaying process, all external calls found in the recording that have
// not been flushed to the root replaying process.
static StaticInfallibleVector<ExternalCall*> gUnflushedCalls;

// In a replaying process, lock protecting external call state. In the
// external process, all accesses occur on the main thread.
static Monitor* gMonitor;

void InitializeExternalCalls() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying() || IsMiddleman());

  if (IsReplaying()) {
    gState = new ExternalCallState();
    gMonitor = new Monitor();
  }
}

static void SetExternalCallValue(ExternalCall* aCall, const void* aValue) {
  aCall->mValue.reset();
  aCall->mValue.emplace(aValue);

  gState->mCallsByValue.erase(aValue);
  gState->mCallsByValue.insert(CallsByValueMap::value_type(aValue, aCall->mId));
}

static void GatherDependentCalls(
    InfallibleVector<ExternalCall*>& aOutgoingCalls, ExternalCall* aCall) {
  for (ExternalCall* existing : aOutgoingCalls) {
    if (existing == aCall) {
      return;
    }
  }

  aOutgoingCalls.append(aCall);

  for (ExternalCallId dependentId : aCall->mDependentCalls) {
    auto iter = gState->mCallsById.find(dependentId);
    MOZ_RELEASE_ASSERT(iter != gState->mCallsById.end());
    ExternalCall* dependent = iter->second;

    GatherDependentCalls(aOutgoingCalls, dependent);
  }
}

bool OnExternalCall(size_t aCallId, CallArguments* aArguments, bool aDiverged) {
  MOZ_RELEASE_ASSERT(IsReplaying());

  const Redirection& redirection = GetRedirection(aCallId);
  MOZ_RELEASE_ASSERT(redirection.mExternalCall);

  const char* messageName = "";
  if (!strcmp(redirection.mName, "objc_msgSend")) {
    messageName = aArguments->Arg<1, const char*>();
  }

  if (aDiverged) {
    PrintSpew("OnExternalCall Diverged %s %s\n", redirection.mName, messageName);
  }

  MonitorAutoLock lock(*gMonitor);

  // Allocate the new ExternalCall.
  ExternalCall* call = new ExternalCall();
  call->mCallId = aCallId;

  // Save all the call's inputs.
  {
    ExternalCallContext cx(call, aArguments,
                           ExternalCallPhase::SaveInput);
    redirection.mExternalCall(cx);
    if (cx.mFailed) {
      delete call;
      if (child::CurrentRepaintCannotFail() && aDiverged) {
        child::ReportFatalError("External call input failed: %s\n",
                                redirection.mName);
      }
      return false;
    }
  }

  call->ComputeId();

  bool isNewCall = false;
  auto iter = gState->mCallsById.find(call->mId);
  if (iter == gState->mCallsById.end()) {
    // We haven't seen this call before.
    isNewCall = true;
    gState->mCallsById.insert(CallsByIdMap::value_type(call->mId, call));
  } else {
    // We've seen this call before, so use the old copy.
    delete call;
    call = iter->second;

    // Reuse this call's result if we need to restore the output.
    if (aDiverged) {
      ExternalCallContext cx(call, aArguments,
                             ExternalCallPhase::RestoreOutput);
      redirection.mExternalCall(cx);
      return true;
    }
  }

  // If we have not diverged from the recording, we already have the outputs
  // we need. Run the SaveOutput phase to capture these so that we can reuse
  // them later and associate any system outputs with the call.
  if (!aDiverged) {
    ExternalCallContext cx(call, aArguments,
                           ExternalCallPhase::SaveOutput);
    redirection.mExternalCall(cx);
    if (isNewCall) {
      gUnflushedCalls.append(call);
    }
    return true;
  }

  PrintSpew("OnExternalCall Send %s %s\n", redirection.mName, messageName);

  // Gather any calls this one transitively depends on.
  InfallibleVector<ExternalCall*> outgoingCalls;
  GatherDependentCalls(outgoingCalls, call);

  // Encode all calls that need to be performed, in the order to perform them.
  InfallibleVector<char> inputData;
  BufferStream inputStream(&inputData);
  for (int i = outgoingCalls.length() - 1; i >= 0; i--) {
    outgoingCalls[i]->EncodeInput(inputStream);
  }

  // Synchronously wait for the call result.
  InfallibleVector<char> outputData;
  child::SendExternalCallRequest(call->mId,
                                 inputData.begin(), inputData.length(),
                                 &outputData);

  // Decode the external call's output.
  BufferStream outputStream(outputData.begin(), outputData.length());
  call->DecodeOutput(outputStream);

  ExternalCallContext cx(call, aArguments, ExternalCallPhase::RestoreOutput);
  redirection.mExternalCall(cx);
  return true;
}

void ProcessExternalCall(const char* aInputData, size_t aInputSize,
                         InfallibleVector<char>* aOutputData) {
  MOZ_RELEASE_ASSERT(IsMiddleman());

  gState = new ExternalCallState();
  auto& calls = gState->mCallsById;

  BufferStream inputStream(aInputData, aInputSize);
  ExternalCall* lastCall = nullptr;

  ExternalCallContext::ReleaseCallbackVector releaseCallbacks;

  while (!inputStream.IsEmpty()) {
    ExternalCall* call = new ExternalCall();
    call->DecodeInput(inputStream);

    const Redirection& redirection = GetRedirection(call->mCallId);
    MOZ_RELEASE_ASSERT(redirection.mExternalCall);

    PrintSpew("ProcessExternalCall %lu %s\n", call->mId, redirection.mName);

    CallArguments arguments;

    bool skipCall;
    {
      ExternalCallContext cx(call, &arguments, ExternalCallPhase::RestoreInput);
      redirection.mExternalCall(cx);
      skipCall = cx.mSkipExecuting;
    }

    if (!skipCall) {
      RecordReplayInvokeCall(redirection.mOriginalFunction, &arguments);
    }

    {
      ExternalCallContext cx(call, &arguments, ExternalCallPhase::SaveOutput);
      cx.mReleaseCallbacks = &releaseCallbacks;
      redirection.mExternalCall(cx);
    }

    lastCall = call;

    MOZ_RELEASE_ASSERT(calls.find(call->mId) == calls.end());
    calls.insert(CallsByIdMap::value_type(call->mId, call));
  }

  BufferStream outputStream(aOutputData);
  lastCall->EncodeOutput(outputStream);

  for (const auto& callback : releaseCallbacks) {
    callback();
  }

  for (auto iter = calls.begin(); iter != calls.end(); ++iter) {
    delete iter->second;
  }

  for (auto buffer : gState->mAllocatedBuffers) {
    free(buffer);
  }

  delete gState;
  gState = nullptr;
}

void* ExternalCallContext::AllocateBytes(size_t aSize) {
  void* rv = malloc(aSize);

  // In an external process, any buffers we allocate live until the calls are
  // reset. In a replaying process, the buffers will live forever, to match the
  // lifetime of the ExternalCall itself.
  if (IsMiddleman()) {
    gState->mAllocatedBuffers.append(rv);
  }

  return rv;
}

void FlushExternalCalls() {
  MonitorAutoLock lock(*gMonitor);

  for (ExternalCall* call : gUnflushedCalls) {
    InfallibleVector<char> outputData;
    BufferStream outputStream(&outputData);
    call->EncodeOutput(outputStream);

    child::SendExternalCallOutput(call->mId, outputData.begin(),
                                  outputData.length());
  }

  gUnflushedCalls.clear();
}

///////////////////////////////////////////////////////////////////////////////
// External Call Caching
///////////////////////////////////////////////////////////////////////////////

// In root replaying processes, the outputs produced by assorted external calls
// are cached for fulfilling future external call requests.
struct ExternalCallOutput {
  char* mOutput;
  size_t mOutputSize;
};

// Protected by gMonitor. Accesses can occur on any thread.
typedef std::unordered_map<ExternalCallId, ExternalCallOutput> CallOutputMap;
static CallOutputMap* gCallOutputMap;

void AddExternalCallOutput(ExternalCallId aId, const char* aOutput,
                           size_t aOutputSize) {
  MonitorAutoLock lock(*gMonitor);

  if (!gCallOutputMap) {
    gCallOutputMap = new CallOutputMap();
  }

  ExternalCallOutput output;
  output.mOutput = new char[aOutputSize];
  memcpy(output.mOutput, aOutput, aOutputSize);
  output.mOutputSize = aOutputSize;
  gCallOutputMap->insert(CallOutputMap::value_type(aId, output));
}

bool HasExternalCallOutput(ExternalCallId aId,
                           InfallibleVector<char>* aOutput) {
  MonitorAutoLock lock(*gMonitor);

  if (!gCallOutputMap) {
    return false;
  }

  auto iter = gCallOutputMap->find(aId);
  if (iter == gCallOutputMap->end()) {
    return false;
  }

  aOutput->append(iter->second.mOutput, iter->second.mOutputSize);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// System Values
///////////////////////////////////////////////////////////////////////////////

static ExternalCall* LookupExternalCall(const void* aThing) {
  CallsByValueMap::const_iterator iter = gState->mCallsByValue.find(aThing);
  if (iter != gState->mCallsByValue.end()) {
    CallsByIdMap::const_iterator iter2 = gState->mCallsById.find(iter->second);
    if (iter2 != gState->mCallsById.end()) {
      return iter2->second;
    }
  }
  return nullptr;
}

static Maybe<const void*> GetExternalCallValue(ExternalCallId aId) {
  auto iter = gState->mCallsById.find(aId);
  if (iter != gState->mCallsById.end()) {
    return iter->second->mValue;
  }
  return Nothing();
}

bool EX_SystemInput(ExternalCallContext& aCx, const void** aThingPtr) {
  MOZ_RELEASE_ASSERT(aCx.AccessInput());

  bool isNull = *aThingPtr == nullptr;
  aCx.ReadOrWriteInputBytes(&isNull, sizeof(isNull));
  if (isNull) {
    *aThingPtr = nullptr;
    return true;
  }

  ExternalCallId callId = 0;
  if (aCx.mPhase == ExternalCallPhase::SaveInput) {
    ExternalCall* call = LookupExternalCall(*aThingPtr);
    if (call) {
      callId = call->mId;
      MOZ_RELEASE_ASSERT(callId);
      aCx.mCall->mDependentCalls.append(call->mId);
    }
  }
  aCx.ReadOrWriteInputBytes(&callId, sizeof(callId));

  if (aCx.mPhase == ExternalCallPhase::RestoreInput) {
    if (callId) {
      Maybe<const void*> value = GetExternalCallValue(callId);
      MOZ_RELEASE_ASSERT(value.isSome());
      *aThingPtr = value.ref();
    }
  }

  return callId != 0;
}

static const void* MangledSystemValue(ExternalCallId aId) {
  return (const void*)((size_t)aId | (1ULL << 63));
}

void EX_SystemOutput(ExternalCallContext& aCx, const void** aOutput,
                     bool aUpdating) {
  if (!aCx.AccessOutput()) {
    return;
  }

  bool isNull = false;
  Maybe<ExternalCallId> aliasedCall;
  if (aCx.mPhase == ExternalCallPhase::SaveOutput) {
    SetExternalCallValue(aCx.mCall, *aOutput);

    isNull = *aOutput == nullptr;
    if (!isNull) {
      ExternalCall* call = LookupExternalCall(*aOutput);
      if (call) {
        aliasedCall.emplace(call->mId);
      }
    }
  }
  aCx.ReadOrWriteOutputBytes(&isNull, sizeof(isNull));
  aCx.ReadOrWriteOutputBytes(&aliasedCall, sizeof(aliasedCall));

  if (aCx.mPhase == ExternalCallPhase::RestoreOutput) {
    do {
      if (isNull) {
        *aOutput = nullptr;
        break;
      }
      if (aliasedCall.isSome()) {
        auto iter = gState->mCallsById.find(aliasedCall.ref());
        if (iter != gState->mCallsById.end()) {
          *aOutput = iter->second;
          break;
        }
        // If we haven't encountered the aliased call, fall through and generate
        // a new value for it. Aliases might be spurious if they were derived from
        // the recording and reflect a value that was released and had its memory
        // reused.
      }
      *aOutput = MangledSystemValue(aCx.mCall->mId);
    } while (false);

    SetExternalCallValue(aCx.mCall, *aOutput);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ExternalCall
///////////////////////////////////////////////////////////////////////////////

void CallReturnRegisters::CopyFrom(CallArguments* aArguments) {
  rval0 = aArguments->Rval<size_t, 0>();
  rval1 = aArguments->Rval<size_t, 1>();
  floatrval0 = aArguments->FloatRval<0>();
  floatrval1 = aArguments->FloatRval<1>();
}

void CallReturnRegisters::CopyTo(CallArguments* aArguments) {
  aArguments->Rval<size_t, 0>() = rval0;
  aArguments->Rval<size_t, 1>() = rval1;
  aArguments->FloatRval<0>() = floatrval0;
  aArguments->FloatRval<1>() = floatrval1;
}

void ExternalCall::EncodeInput(BufferStream& aStream) const {
  aStream.WriteScalar(mId);
  aStream.WriteScalar(mCallId);
  aStream.WriteScalar(mExcludeInput);
  aStream.WriteScalar(mInput.length());
  aStream.WriteBytes(mInput.begin(), mInput.length());
}

void ExternalCall::DecodeInput(BufferStream& aStream) {
  mId = aStream.ReadScalar();
  mCallId = aStream.ReadScalar();
  mExcludeInput = aStream.ReadScalar();
  size_t inputLength = aStream.ReadScalar();
  mInput.appendN(0, inputLength);
  aStream.ReadBytes(mInput.begin(), inputLength);
}

void ExternalCall::EncodeOutput(BufferStream& aStream) const {
  aStream.WriteBytes(&mReturnRegisters, sizeof(CallReturnRegisters));
  aStream.WriteScalar(mOutput.length());
  aStream.WriteBytes(mOutput.begin(), mOutput.length());
}

void ExternalCall::DecodeOutput(BufferStream& aStream) {
  aStream.ReadBytes(&mReturnRegisters, sizeof(CallReturnRegisters));

  size_t outputLength = aStream.ReadScalar();
  mOutput.appendN(0, outputLength);
  aStream.ReadBytes(mOutput.begin(), outputLength);
}

}  // namespace recordreplay
}  // namespace mozilla
