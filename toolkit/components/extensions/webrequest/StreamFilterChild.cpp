/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamFilterChild.h"
#include "StreamFilter.h"

#include "mozilla/Assertions.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace extensions {

using mozilla::dom::StreamFilterStatus;
using mozilla::ipc::IPCResult;

/*****************************************************************************
 * Initialization and cleanup
 *****************************************************************************/

void
StreamFilterChild::Cleanup()
{
  switch (mState) {
  case State::Closing:
  case State::Closed:
  case State::Error:
  case State::Disconnecting:
  case State::Disconnected:
    break;

  default:
    ErrorResult rv;
    Disconnect(rv);
    break;
  }
}

/*****************************************************************************
 * State change methods
 *****************************************************************************/

void
StreamFilterChild::Suspend(ErrorResult& aRv)
{
  switch (mState) {
  case State::TransferringData:
    mState = State::Suspending;
    mNextState = State::Suspended;

    SendSuspend();
    break;

  case State::Suspending:
    switch (mNextState) {
    case State::Suspended:
    case State::Resuming:
      mNextState = State::Suspended;
      break;

    default:
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    break;

  case State::Resuming:
    switch (mNextState) {
    case State::TransferringData:
    case State::Suspending:
      mNextState = State::Suspending;
      break;

    default:
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    break;

  case State::Suspended:
    break;

  default:
    aRv.Throw(NS_ERROR_FAILURE);
    break;
  }
}

void
StreamFilterChild::Resume(ErrorResult& aRv)
{
  switch (mState) {
  case State::Suspended:
    mState = State::Resuming;
    mNextState = State::TransferringData;

    SendResume();
    break;

  case State::Suspending:
    switch (mNextState) {
    case State::Suspended:
    case State::Resuming:
      mNextState = State::Resuming;
      break;

    default:
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    break;

  case State::Resuming:
  case State::TransferringData:
    break;

  default:
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  FlushBufferedData();
}

void
StreamFilterChild::Disconnect(ErrorResult& aRv)
{
  switch (mState) {
  case State::Suspended:
  case State::TransferringData:
  case State::FinishedTransferringData:
    mState = State::Disconnecting;
    mNextState = State::Disconnected;

    SendDisconnect();
    break;

  case State::Suspending:
  case State::Resuming:
    switch (mNextState) {
    case State::Suspended:
    case State::Resuming:
    case State::Disconnecting:
      mNextState = State::Disconnecting;
      break;

    default:
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    break;

  case State::Disconnecting:
  case State::Disconnected:
    break;

  default:
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
StreamFilterChild::Close(ErrorResult& aRv)
{
  switch (mState) {
  case State::Suspended:
  case State::TransferringData:
  case State::FinishedTransferringData:
    mState = State::Closing;
    mNextState = State::Closed;

    SendClose();
    break;

  case State::Suspending:
  case State::Resuming:
    mNextState = State::Closing;
    break;

  case State::Closing:
    MOZ_DIAGNOSTIC_ASSERT(mNextState == State::Closed);
    break;

  case State::Closed:
    break;

  default:
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  mBufferedData.clear();
}

/*****************************************************************************
 * Internal state management
 *****************************************************************************/

void
StreamFilterChild::SetNextState()
{
  mState = mNextState;

  switch (mNextState) {
  case State::Suspending:
    mNextState = State::Suspended;
    SendSuspend();
    break;

  case State::Resuming:
    mNextState = State::TransferringData;
    SendResume();
    break;

  case State::Closing:
    mNextState = State::Closed;
    SendClose();
    break;

  case State::Disconnecting:
    mNextState = State::Disconnected;
    SendDisconnect();
    break;

  case State::FinishedTransferringData:
    if (mStreamFilter) {
      mStreamFilter->FireEvent(NS_LITERAL_STRING("stop"));
      // We don't need access to the stream filter after this point, so break our
      // reference cycle, so that it can be collected if we're the last reference.
      mStreamFilter = nullptr;
    }
    break;

  case State::TransferringData:
    FlushBufferedData();
    break;

  case State::Closed:
  case State::Disconnected:
  case State::Error:
    mStreamFilter = nullptr;
    break;

  default:
    break;
  }
}

void
StreamFilterChild::MaybeStopRequest()
{
  if (!mReceivedOnStop || !mBufferedData.isEmpty()) {
    return;
  }

  switch (mState) {
  case State::Suspending:
  case State::Resuming:
    mNextState = State::FinishedTransferringData;
    return;

  default:
    mState = State::FinishedTransferringData;
    if (mStreamFilter) {
      mStreamFilter->FireEvent(NS_LITERAL_STRING("stop"));
      // We don't need access to the stream filter after this point, so break our
      // reference cycle, so that it can be collected if we're the last reference.
      mStreamFilter = nullptr;
    }
    break;
  }
}

/*****************************************************************************
 * State change acknowledgment callbacks
 *****************************************************************************/

void
StreamFilterChild::RecvInitialized(bool aSuccess)
{
  MOZ_ASSERT(mState == State::Uninitialized);

  if (aSuccess) {
    mState = State::Initialized;
  } else {
    mState = State::Error;
    if (mStreamFilter) {
      mStreamFilter->FireErrorEvent(NS_LITERAL_STRING("Invalid request ID"));
      mStreamFilter = nullptr;
    }
  }
}

IPCResult
StreamFilterChild::RecvClosed() {
  MOZ_DIAGNOSTIC_ASSERT(mState == State::Closing);

  SetNextState();
  return IPC_OK();
}

IPCResult
StreamFilterChild::RecvSuspended() {
  MOZ_DIAGNOSTIC_ASSERT(mState == State::Suspending);

  SetNextState();
  return IPC_OK();
}

IPCResult
StreamFilterChild::RecvResumed() {
  MOZ_DIAGNOSTIC_ASSERT(mState == State::Resuming);

  SetNextState();
  return IPC_OK();
}

IPCResult
StreamFilterChild::RecvFlushData() {
  MOZ_DIAGNOSTIC_ASSERT(mState == State::Disconnecting);

  SendFlushedData();
  SetNextState();
  return IPC_OK();
}

/*****************************************************************************
 * Other binding methods
 *****************************************************************************/

void
StreamFilterChild::Write(Data&& aData, ErrorResult& aRv)
{
  switch (mState) {
  case State::Suspending:
  case State::Resuming:
    switch (mNextState) {
    case State::Suspended:
    case State::TransferringData:
      break;

    default:
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    break;

  case State::Suspended:
  case State::TransferringData:
  case State::FinishedTransferringData:
    break;

  default:
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  SendWrite(Move(aData));
}

StreamFilterStatus
StreamFilterChild::Status() const
{
  switch (mState) {
  case State::Uninitialized:
  case State::Initialized:
    return StreamFilterStatus::Uninitialized;

  case State::TransferringData:
    return StreamFilterStatus::Transferringdata;

  case State::Suspended:
    return StreamFilterStatus::Suspended;

  case State::FinishedTransferringData:
    return StreamFilterStatus::Finishedtransferringdata;

  case State::Resuming:
  case State::Suspending:
    switch (mNextState) {
    case State::TransferringData:
    case State::Resuming:
      return StreamFilterStatus::Transferringdata;

    case State::Suspended:
    case State::Suspending:
      return StreamFilterStatus::Suspended;

    case State::Closing:
      return StreamFilterStatus::Closed;

    case State::Disconnecting:
      return StreamFilterStatus::Disconnected;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected next state");
      return StreamFilterStatus::Suspended;
    }
    break;

  case State::Closing:
  case State::Closed:
    return StreamFilterStatus::Closed;

  case State::Disconnecting:
  case State::Disconnected:
    return StreamFilterStatus::Disconnected;

  case State::Error:
    return StreamFilterStatus::Failed;
  };

  MOZ_ASSERT_UNREACHABLE("Not reached");
  return StreamFilterStatus::Failed;
}

/*****************************************************************************
 * Request state notifications
 *****************************************************************************/

IPCResult
StreamFilterChild::RecvStartRequest()
{
  MOZ_ASSERT(mState == State::Initialized);

  mState = State::TransferringData;

  if (mStreamFilter) {
    mStreamFilter->FireEvent(NS_LITERAL_STRING("start"));
  }
  return IPC_OK();
}

IPCResult
StreamFilterChild::RecvStopRequest(const nsresult& aStatus)
{
  mReceivedOnStop = true;
  MaybeStopRequest();
  return IPC_OK();
}

/*****************************************************************************
 * Incoming request data handling
 *****************************************************************************/

void
StreamFilterChild::EmitData(const Data& aData)
{
  MOZ_ASSERT(CanFlushData());
  if (mStreamFilter) {
    mStreamFilter->FireDataEvent(aData);
  }

  MaybeStopRequest();
}

void
StreamFilterChild::FlushBufferedData()
{
  while (!mBufferedData.isEmpty() && CanFlushData()) {
    UniquePtr<BufferedData> data(mBufferedData.popFirst());

    EmitData(data->mData);
  }
}

IPCResult
StreamFilterChild::RecvData(Data&& aData)
{
  MOZ_ASSERT(!mReceivedOnStop);

  switch (mState) {
  case State::TransferringData:
  case State::Resuming:
    EmitData(aData);
    break;

  case State::FinishedTransferringData:
    MOZ_ASSERT_UNREACHABLE("Received data in unexpected state");
    EmitData(aData);
    break;

  case State::Suspending:
  case State::Suspended:
    BufferData(Move(aData));
    break;

  case State::Disconnecting:
    SendWrite(Move(aData));
    break;

  case State::Closing:
    break;

  default:
    MOZ_ASSERT_UNREACHABLE("Received data in unexpected state");
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

void
StreamFilterChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mStreamFilter = nullptr;
}

void
StreamFilterChild::DeallocPStreamFilterChild()
{
  RefPtr<StreamFilterChild> self = dont_AddRef(this);
}

} // namespace extensions
} // namespace mozilla
