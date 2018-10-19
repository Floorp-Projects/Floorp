/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MiddlemanCall.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

// In a replaying or middleman process, all middleman calls that have been
// encountered, indexed by their ID.
static StaticInfallibleVector<MiddlemanCall*> gMiddlemanCalls;

// In a replaying or middleman process, association between values produced by
// a middleman call and the call itself.
typedef std::unordered_map<const void*, MiddlemanCall*> MiddlemanCallMap;
static MiddlemanCallMap* gMiddlemanCallMap;

// In a middleman process, any buffers allocated for performed calls.
static StaticInfallibleVector<void*> gAllocatedBuffers;

// Lock protecting middleman call state.
static Monitor* gMonitor;

void
InitializeMiddlemanCalls()
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying() || IsMiddleman());

  gMiddlemanCallMap = new MiddlemanCallMap();
  gMonitor = new Monitor();
}

// Apply the ReplayInput phase to aCall and any calls it depends on that have
// not been sent to the middleman yet, filling aOutgoingCalls with the set of
// such calls.
static bool
GatherDependentCalls(InfallibleVector<MiddlemanCall*>& aOutgoingCalls, MiddlemanCall* aCall)
{
  MOZ_RELEASE_ASSERT(!aCall->mSent);
  aCall->mSent = true;

  CallArguments arguments;
  aCall->mArguments.CopyTo(&arguments);

  InfallibleVector<MiddlemanCall*> dependentCalls;

  MiddlemanCallContext cx(aCall, &arguments, MiddlemanCallPhase::ReplayInput);
  cx.mDependentCalls = &dependentCalls;
  gRedirections[aCall->mCallId].mMiddlemanCall(cx);
  if (cx.mFailed) {
    return false;
  }

  for (MiddlemanCall* dependent : dependentCalls) {
    if (!dependent->mSent && !GatherDependentCalls(aOutgoingCalls, dependent)) {
      return false;
    }
  }

  aOutgoingCalls.append(aCall);
  return true;
}

bool
SendCallToMiddleman(size_t aCallId, CallArguments* aArguments, bool aDiverged)
{
  MOZ_RELEASE_ASSERT(IsReplaying());

  const Redirection& redirection = gRedirections[aCallId];
  MOZ_RELEASE_ASSERT(redirection.mMiddlemanCall);

  Maybe<MonitorAutoLock> lock;
  lock.emplace(*gMonitor);

  // Allocate and fill in a new MiddlemanCall.
  size_t id = gMiddlemanCalls.length();
  MiddlemanCall* newCall = new MiddlemanCall();
  gMiddlemanCalls.emplaceBack(newCall);
  newCall->mId = id;
  newCall->mCallId = aCallId;
  newCall->mArguments.CopyFrom(aArguments);

  // Perform the ReplayPreface phase on the new call.
  {
    MiddlemanCallContext cx(newCall, aArguments, MiddlemanCallPhase::ReplayPreface);
    redirection.mMiddlemanCall(cx);
    if (cx.mFailed) {
      delete newCall;
      gMiddlemanCalls.popBack();
      return false;
    }
  }

  // Other phases will not run if we have not diverged from the recording.
  // Any outputs for the call have been handled by the SaveOutput hook.
  if (!aDiverged) {
    return true;
  }

  // Perform the ReplayInput phase on the new call and any others it depends on.
  InfallibleVector<MiddlemanCall*> outgoingCalls;
  if (!GatherDependentCalls(outgoingCalls, newCall)) {
    for (MiddlemanCall* call : outgoingCalls) {
      call->mSent = false;
    }
    return false;
  }

  // Encode all calls we are sending to the middleman.
  InfallibleVector<char> inputData;
  BufferStream inputStream(&inputData);
  for (MiddlemanCall* call : outgoingCalls) {
    call->EncodeInput(inputStream);
  }

  // Perform the calls synchronously in the middleman.
  InfallibleVector<char> outputData;
  if (!child::SendMiddlemanCallRequest(inputData.begin(), inputData.length(), &outputData)) {
    // This thread is not allowed to perform middleman calls anymore. Release
    // the lock and block until the process rewinds.
    lock.reset();
    Thread::WaitForever();
  }

  // Decode outputs for the calls just sent, and perform the ReplayOutput phase
  // on any older dependent calls we sent.
  BufferStream outputStream(outputData.begin(), outputData.length());
  for (MiddlemanCall* call : outgoingCalls) {
    call->DecodeOutput(outputStream);

    if (call != newCall) {
      CallArguments oldArguments;
      call->mArguments.CopyTo(&oldArguments);
      MiddlemanCallContext cx(call, &oldArguments, MiddlemanCallPhase::ReplayOutput);
      cx.mReplayOutputIsOld = true;
      gRedirections[call->mCallId].mMiddlemanCall(cx);
    }
  }

  // Perform the ReplayOutput phase to fill in outputs for the current call.
  newCall->mArguments.CopyTo(aArguments);
  MiddlemanCallContext cx(newCall, aArguments, MiddlemanCallPhase::ReplayOutput);
  redirection.mMiddlemanCall(cx);
  return true;
}

void
ProcessMiddlemanCall(const char* aInputData, size_t aInputSize,
                     InfallibleVector<char>* aOutputData)
{
  MOZ_RELEASE_ASSERT(IsMiddleman());

  BufferStream inputStream(aInputData, aInputSize);
  BufferStream outputStream(aOutputData);

  while (!inputStream.IsEmpty()) {
    MiddlemanCall* call = new MiddlemanCall();
    call->DecodeInput(inputStream);

    const Redirection& redirection = gRedirections[call->mCallId];
    MOZ_RELEASE_ASSERT(gRedirections[call->mCallId].mMiddlemanCall);

    CallArguments arguments;
    call->mArguments.CopyTo(&arguments);

    {
      MiddlemanCallContext cx(call, &arguments, MiddlemanCallPhase::MiddlemanInput);
      redirection.mMiddlemanCall(cx);
    }

    RecordReplayInvokeCall(call->mCallId, &arguments);

    {
      MiddlemanCallContext cx(call, &arguments, MiddlemanCallPhase::MiddlemanOutput);
      redirection.mMiddlemanCall(cx);
    }

    call->mArguments.CopyFrom(&arguments);
    call->EncodeOutput(outputStream);

    while (call->mId >= gMiddlemanCalls.length()) {
      gMiddlemanCalls.emplaceBack(nullptr);
    }
    MOZ_RELEASE_ASSERT(!gMiddlemanCalls[call->mId]);
    gMiddlemanCalls[call->mId] = call;
  }
}

void*
MiddlemanCallContext::AllocateBytes(size_t aSize)
{
  void* rv = malloc(aSize);

  // In a middleman process, any buffers we allocate live until the calls are
  // reset. In a replaying process, the buffers will either live forever
  // (if they are allocated in the ReplayPreface phase, to match the lifetime
  // of the MiddlemanCall itself) or will be recovered when we rewind after we
  // are done with our divergence from the recording (any other phase).
  if (IsMiddleman()) {
    gAllocatedBuffers.append(rv);
  }

  return rv;
}

void
ResetMiddlemanCalls()
{
  MOZ_RELEASE_ASSERT(IsMiddleman());

  for (MiddlemanCall* call : gMiddlemanCalls) {
    if (call) {
      CallArguments arguments;
      call->mArguments.CopyTo(&arguments);

      MiddlemanCallContext cx(call, &arguments, MiddlemanCallPhase::MiddlemanRelease);
      gRedirections[call->mCallId].mMiddlemanCall(cx);

      delete call;
    }
  }

  gMiddlemanCalls.clear();
  for (auto buffer : gAllocatedBuffers) {
    free(buffer);
  }
  gAllocatedBuffers.clear();
}

///////////////////////////////////////////////////////////////////////////////
// System Values
///////////////////////////////////////////////////////////////////////////////

static void
AddMiddlemanCallValue(const void* aThing, MiddlemanCall* aCall)
{
  gMiddlemanCallMap->erase(aThing);
  gMiddlemanCallMap->insert(MiddlemanCallMap::value_type(aThing, aCall));
}

static MiddlemanCall*
LookupMiddlemanCall(const void* aThing)
{
  MiddlemanCallMap::const_iterator iter = gMiddlemanCallMap->find(aThing);
  if (iter != gMiddlemanCallMap->end()) {
    return iter->second;
  }
  return nullptr;
}

static const void*
GetMiddlemanCallValue(size_t aId)
{
  MOZ_RELEASE_ASSERT(IsMiddleman());
  MOZ_RELEASE_ASSERT(aId < gMiddlemanCalls.length() &&
                     gMiddlemanCalls[aId] &&
                     gMiddlemanCalls[aId]->mMiddlemanValue.isSome());
  return gMiddlemanCalls[aId]->mMiddlemanValue.ref();
}

bool
Middleman_SystemInput(MiddlemanCallContext& aCx, const void** aThingPtr)
{
  MOZ_RELEASE_ASSERT(aCx.AccessPreface());

  if (!*aThingPtr) {
    // Null values are handled by the normal argument copying logic.
    return true;
  }

  Maybe<size_t> callId;
  if (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) {
    // Determine any middleman call this object came from, before the pointer
    // has a chance to be clobbered by another call between this and the
    // ReplayInput phase.
    MiddlemanCall* call = LookupMiddlemanCall(*aThingPtr);
    if (call) {
      callId.emplace(call->mId);
    }
  }
  aCx.ReadOrWritePrefaceBytes(&callId, sizeof(callId));

  switch (aCx.mPhase) {
  case MiddlemanCallPhase::ReplayPreface:
    return true;
  case MiddlemanCallPhase::ReplayInput:
    if (callId.isSome()) {
      aCx.WriteInputScalar(callId.ref());
      aCx.mDependentCalls->append(gMiddlemanCalls[callId.ref()]);
      return true;
    }
    return false;
  case MiddlemanCallPhase::MiddlemanInput:
    if (callId.isSome()) {
      size_t callIndex = aCx.ReadInputScalar();
      *aThingPtr = GetMiddlemanCallValue(callIndex);
      return true;
    }
    return false;
  default:
    MOZ_CRASH("Bad phase");
  }
}

// Pointer system values are preserved during the replay so that null tests
// and equality tests work as expected. We additionally mangle the
// pointers here by setting one of the two highest bits, depending on whether
// the pointer came from the recording or from the middleman. This avoids
// accidentally conflating pointers that happen to have the same value but
// which originate from different processes.
static const void*
MangleSystemValue(const void* aValue, bool aFromRecording)
{
  return (const void*) ((size_t)aValue | (1ULL << (aFromRecording ? 63 : 62)));
}

void
Middleman_SystemOutput(MiddlemanCallContext& aCx, const void** aOutput, bool aUpdating)
{
  if (!*aOutput) {
    return;
  }

  switch (aCx.mPhase) {
  case MiddlemanCallPhase::ReplayPreface:
    if (!HasDivergedFromRecording()) {
      // If we haven't diverged from the recording, use the output value saved
      // in the recording.
      if (!aUpdating) {
        *aOutput = MangleSystemValue(*aOutput, true);
      }
      aCx.mCall->SetRecordingValue(*aOutput);
      AddMiddlemanCallValue(*aOutput, aCx.mCall);
    }
    break;
  case MiddlemanCallPhase::MiddlemanOutput:
    aCx.mCall->SetMiddlemanValue(*aOutput);
    AddMiddlemanCallValue(*aOutput, aCx.mCall);
    break;
  case MiddlemanCallPhase::ReplayOutput: {
    if (!aUpdating) {
      *aOutput = MangleSystemValue(*aOutput, false);
    }
    aCx.mCall->SetMiddlemanValue(*aOutput);

    // Associate the value produced by the middleman with this call. If the
    // call previously went through the ReplayPreface phase when we did not
    // diverge from the recording, we will associate values from both the
    // recording and middleman processes with this call. If a call made after
    // diverging produced the same value as a call made before diverging, use
    // the value saved in the recording for the first call, so that equality
    // tests on the value work as expected.
    MiddlemanCall* previousCall = LookupMiddlemanCall(*aOutput);
    if (previousCall) {
      if (previousCall->mRecordingValue.isSome()) {
        *aOutput = previousCall->mRecordingValue.ref();
      }
    } else {
      AddMiddlemanCallValue(*aOutput, aCx.mCall);
    }
    break;
  }
  default:
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MiddlemanCall
///////////////////////////////////////////////////////////////////////////////

void
MiddlemanCall::EncodeInput(BufferStream& aStream) const
{
  aStream.WriteScalar(mId);
  aStream.WriteScalar(mCallId);
  aStream.WriteBytes(&mArguments, sizeof(CallRegisterArguments));
  aStream.WriteScalar(mPreface.length());
  aStream.WriteBytes(mPreface.begin(), mPreface.length());
  aStream.WriteScalar(mInput.length());
  aStream.WriteBytes(mInput.begin(), mInput.length());
}

void
MiddlemanCall::DecodeInput(BufferStream& aStream)
{
  mId = aStream.ReadScalar();
  mCallId = aStream.ReadScalar();
  aStream.ReadBytes(&mArguments, sizeof(CallRegisterArguments));
  size_t prefaceLength = aStream.ReadScalar();
  mPreface.appendN(0, prefaceLength);
  aStream.ReadBytes(mPreface.begin(), prefaceLength);
  size_t inputLength = aStream.ReadScalar();
  mInput.appendN(0, inputLength);
  aStream.ReadBytes(mInput.begin(), inputLength);
}

void
MiddlemanCall::EncodeOutput(BufferStream& aStream) const
{
  aStream.WriteBytes(&mArguments, sizeof(CallRegisterArguments));
  aStream.WriteScalar(mOutput.length());
  aStream.WriteBytes(mOutput.begin(), mOutput.length());
}

void
MiddlemanCall::DecodeOutput(BufferStream& aStream)
{
  // Only update the return value when decoding arguments, so that we don't
  // clobber the call's arguments with any changes made in the middleman.
  CallRegisterArguments newArguments;
  aStream.ReadBytes(&newArguments, sizeof(CallRegisterArguments));
  mArguments.CopyRvalFrom(&newArguments);

  size_t outputLength = aStream.ReadScalar();
  mOutput.appendN(0, outputLength);
  aStream.ReadBytes(mOutput.begin(), outputLength);
}

} // namespace recordreplay
} // namespace mozilla
