/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamFilterParent.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/nsIContentParent.h"
#include "nsIChannel.h"
#include "nsITraceableChannel.h"
#include "nsProxyRelease.h"
#include "nsStringStream.h"

namespace mozilla {
namespace extensions {

/*****************************************************************************
 * Initialization
 *****************************************************************************/

StreamFilterParent::StreamFilterParent(uint64_t aChannelId, const nsAString& aAddonId)
  : mChannelId(aChannelId)
  , mAddonId(NS_Atomize(aAddonId))
  , mPBackgroundThread(NS_GetCurrentThread())
  , mIOThread(do_GetMainThread())
  , mBufferMutex("StreamFilter buffer mutex")
  , mReceivedStop(false)
  , mSentStop(false)
  , mState(State::Uninitialized)
{
}

StreamFilterParent::~StreamFilterParent()
{
}

void
StreamFilterParent::Init(already_AddRefed<nsIContentParent> aContentParent)
{
  AssertIsPBackgroundThread();

  SystemGroup::Dispatch(
    TaskCategory::Network,
    NewRunnableMethod<already_AddRefed<nsIContentParent>&&>(
        "StreamFilterParent::DoInit",
        this, &StreamFilterParent::DoInit, Move(aContentParent)));
}

void
StreamFilterParent::DoInit(already_AddRefed<nsIContentParent>&& aContentParent)
{
  AssertIsMainThread();

  nsCOMPtr<nsIContentParent> contentParent = aContentParent;

  bool success = false;
  auto guard = MakeScopeExit([&] {
    RefPtr<StreamFilterParent> self(this);

    RunOnPBackgroundThread(FUNC, [=] {
      if (self->IPCActive()) {
        self->mState = State::Initialized;
        self->CheckResult(self->SendInitialized(success));
      }
    });
  });

  auto& webreq = WebRequestService::GetSingleton();

  mChannel = webreq.GetTraceableChannel(mChannelId, mAddonId, contentParent);
  if (NS_WARN_IF(!mChannel)) {
    return;
  }

  nsCOMPtr<nsITraceableChannel> traceable = do_QueryInterface(mChannel);
  MOZ_RELEASE_ASSERT(traceable);
}

/*****************************************************************************
 * Error handling
 *****************************************************************************/

void
StreamFilterParent::Broken()
{
  AssertIsPBackgroundThread();

  mState = State::Disconnecting;

  RefPtr<StreamFilterParent> self(this);
  RunOnIOThread(FUNC, [=] {
    self->FlushBufferedData();

    RunOnPBackgroundThread(FUNC, [=] {
      if (self->IPCActive()) {
        self->mState = State::Disconnected;
      }
    });
  });
}

/*****************************************************************************
 * State change requests
 *****************************************************************************/

IPCResult
StreamFilterParent::RecvClose()
{
  AssertIsPBackgroundThread();

  mState = State::Closed;

  if (!mSentStop) {
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      nsresult rv = self->EmitStopRequest(NS_OK);
      Unused << NS_WARN_IF(NS_FAILED(rv));
    });
  }

  Unused << SendClosed();
  Unused << Send__delete__(this);
  return IPC_OK();
}

IPCResult
StreamFilterParent::RecvSuspend()
{
  AssertIsPBackgroundThread();

  if (mState == State::TransferringData) {
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      self->mChannel->Suspend();

      RunOnPBackgroundThread(FUNC, [=] {
        if (self->IPCActive()) {
          self->mState = State::Suspended;
          self->CheckResult(self->SendSuspended());
        }
      });
    });
  }
  return IPC_OK();
}

IPCResult
StreamFilterParent::RecvResume()
{
  AssertIsPBackgroundThread();

  if (mState == State::Suspended) {
    // Change state before resuming so incoming data is handled correctly
    // immediately after resuming.
    mState = State::TransferringData;

    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      self->mChannel->Resume();

      RunOnPBackgroundThread(FUNC, [=] {
        if (self->IPCActive()) {
          self->CheckResult(self->SendResumed());
        }
      });
    });
  }
  return IPC_OK();
}

IPCResult
StreamFilterParent::RecvDisconnect()
{
  AssertIsPBackgroundThread();

  if (mState == State::Suspended) {
  RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      self->mChannel->Resume();
    });
  } else if (mState != State::TransferringData) {
    return IPC_OK();
  }

  mState = State::Disconnecting;
  CheckResult(SendFlushData());
  return IPC_OK();
}

IPCResult
StreamFilterParent::RecvFlushedData()
{
  AssertIsPBackgroundThread();

  MOZ_ASSERT(mState == State::Disconnecting);

  Unused << Send__delete__(this);

  RefPtr<StreamFilterParent> self(this);
  RunOnIOThread(FUNC, [=] {
    self->FlushBufferedData();

    RunOnPBackgroundThread(FUNC, [=] {
      self->mState = State::Disconnected;
    });
  });
  return IPC_OK();
}

/*****************************************************************************
 * Data output
 *****************************************************************************/

IPCResult
StreamFilterParent::RecvWrite(Data&& aData)
{
  AssertIsPBackgroundThread();

  mIOThread->Dispatch(
    NewRunnableMethod<Data&&>("StreamFilterParent::WriteMove",
                              this,
                              &StreamFilterParent::WriteMove,
                              Move(aData)),
    NS_DISPATCH_NORMAL);
  return IPC_OK();
}

void
StreamFilterParent::WriteMove(Data&& aData)
{
  nsresult rv = Write(aData);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

nsresult
StreamFilterParent::Write(Data& aData)
{
  AssertIsIOThread();

  return NS_OK;
}

/*****************************************************************************
 * Incoming data handling
 *****************************************************************************/

void
StreamFilterParent::DoSendData(Data&& aData)
{
  AssertIsPBackgroundThread();

  if (mState == State::TransferringData) {
  }
}

nsresult
StreamFilterParent::FlushBufferedData()
{
  AssertIsIOThread();

  // When offloading data to a thread pool, OnDataAvailable isn't guaranteed
  // to always run in the same thread, so it's possible for this function to
  // run in parallel with OnDataAvailable.
  MutexAutoLock al(mBufferMutex);

  while (!mBufferedData.isEmpty()) {
    UniquePtr<BufferedData> data(mBufferedData.popFirst());

    nsresult rv = Write(data->mData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mReceivedStop && !mSentStop) {
  }

  return NS_OK;
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

void
StreamFilterParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsPBackgroundThread();

  if (mState != State::Disconnected && mState != State::Closed) {
    Broken();
  }
}

NS_IMPL_ISUPPORTS0(StreamFilterParent)

} // namespace extensions
} // namespace mozilla

