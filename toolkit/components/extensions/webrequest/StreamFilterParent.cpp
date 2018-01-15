/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamFilterParent.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsHttpChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInputStream.h"
#include "nsITraceableChannel.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSocketTransportService2.h"
#include "nsStringStream.h"

namespace mozilla {
namespace extensions {

/*****************************************************************************
 * Event queueing helpers
 *****************************************************************************/

using net::ChannelEvent;
using net::ChannelEventQueue;

namespace {

// Define some simple ChannelEvent sub-classes that store the appropriate
// EventTarget and delegate their Run methods to a wrapped Runnable or lambda
// function.

class ChannelEventWrapper : public ChannelEvent
{
public:
  ChannelEventWrapper(nsIEventTarget* aTarget)
    : mTarget(aTarget)
  {}

  already_AddRefed<nsIEventTarget> GetEventTarget() override
  {
    return do_AddRef(mTarget);
  }

protected:
  ~ChannelEventWrapper() override = default;

private:
  nsCOMPtr<nsIEventTarget> mTarget;
};

class ChannelEventFunction final : public ChannelEventWrapper
{
public:
  ChannelEventFunction(nsIEventTarget* aTarget, std::function<void()>&& aFunc)
    : ChannelEventWrapper(aTarget)
    , mFunc(Move(aFunc))
  {}

  void Run() override
  {
    mFunc();
  }

protected:
  ~ChannelEventFunction() override = default;

private:
  std::function<void()> mFunc;
};

class ChannelEventRunnable final : public ChannelEventWrapper
{
public:
  ChannelEventRunnable(nsIEventTarget* aTarget, already_AddRefed<Runnable> aRunnable)
    : ChannelEventWrapper(aTarget)
    , mRunnable(aRunnable)
  {}

  void Run() override
  {
    nsresult rv = mRunnable->Run();
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

protected:
  ~ChannelEventRunnable() override = default;

private:
  RefPtr<Runnable> mRunnable;
};

} // anonymous namespace

/*****************************************************************************
 * Initialization
 *****************************************************************************/

StreamFilterParent::StreamFilterParent()
  : mMainThread(GetCurrentThreadEventTarget())
  , mIOThread(mMainThread)
  , mQueue(new ChannelEventQueue(static_cast<nsIStreamListener*>(this)))
  , mBufferMutex("StreamFilter buffer mutex")
  , mReceivedStop(false)
  , mSentStop(false)
  , mContext(nullptr)
  , mOffset(0)
  , mState(State::Uninitialized)
{}

StreamFilterParent::~StreamFilterParent()
{
  NS_ReleaseOnMainThreadSystemGroup("StreamFilterParent::mOrigListener",
                                    mOrigListener.forget());
  NS_ReleaseOnMainThreadSystemGroup("StreamFilterParent::mContext",
                                    mContext.forget());
}

bool
StreamFilterParent::Create(dom::ContentParent* aContentParent, uint64_t aChannelId, const nsAString& aAddonId,
                           Endpoint<PStreamFilterChild>* aEndpoint)
{
  AssertIsMainThread();

  auto& webreq = WebRequestService::GetSingleton();

  RefPtr<nsAtom> addonId = NS_Atomize(aAddonId);
  nsCOMPtr<nsITraceableChannel> channel = webreq.GetTraceableChannel(aChannelId, addonId, aContentParent);

  RefPtr<nsHttpChannel> chan = do_QueryObject(channel);
  NS_ENSURE_TRUE(chan, false);

  auto channelPid = chan->ProcessId();
  NS_ENSURE_TRUE(channelPid, false);

  Endpoint<PStreamFilterParent> parent;
  Endpoint<PStreamFilterChild> child;
  nsresult rv = PStreamFilter::CreateEndpoints(channelPid,
                                               aContentParent ? aContentParent->OtherPid()
                                                              : base::GetCurrentProcId(),
                                               &parent, &child);
  NS_ENSURE_SUCCESS(rv, false);

  if (!chan->AttachStreamFilter(Move(parent))) {
    return false;
  }

  *aEndpoint = Move(child);
  return true;
}

/* static */ void
StreamFilterParent::Attach(nsIChannel* aChannel, ParentEndpoint&& aEndpoint)
{
  auto self = MakeRefPtr<StreamFilterParent>();

  self->ActorThread()->Dispatch(
    NewRunnableMethod<ParentEndpoint&&>("StreamFilterParent::Bind",
                                        self,
                                        &StreamFilterParent::Bind,
                                        Move(aEndpoint)),
    NS_DISPATCH_NORMAL);

  self->Init(aChannel);

  // IPC owns this reference now.
  Unused << self.forget();
}

void
StreamFilterParent::Bind(ParentEndpoint&& aEndpoint)
{
  aEndpoint.Bind(this);
}

void
StreamFilterParent::Init(nsIChannel* aChannel)
{
  mChannel = aChannel;

  nsCOMPtr<nsITraceableChannel> traceable = do_QueryInterface(aChannel);
  MOZ_RELEASE_ASSERT(traceable);

  nsresult rv = traceable->SetNewListener(this, getter_AddRefs(mOrigListener));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

/*****************************************************************************
 * nsIThreadRetargetableStreamListener
 *****************************************************************************/

NS_IMETHODIMP
StreamFilterParent::CheckListenerChain()
{
  AssertIsMainThread();

  nsCOMPtr<nsIThreadRetargetableStreamListener> trsl =
    do_QueryInterface(mOrigListener);
  if (trsl) {
    return trsl->CheckListenerChain();
  }
  return NS_ERROR_FAILURE;
}

/*****************************************************************************
 * Error handling
 *****************************************************************************/

void
StreamFilterParent::Broken()
{
  AssertIsActorThread();

  mState = State::Disconnecting;

  RefPtr<StreamFilterParent> self(this);
  RunOnIOThread(FUNC, [=] {
    self->FlushBufferedData();

    RunOnActorThread(FUNC, [=] {
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
  AssertIsActorThread();

  mState = State::Closed;

  if (!mSentStop) {
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      nsresult rv = self->EmitStopRequest(NS_OK);
      Unused << NS_WARN_IF(NS_FAILED(rv));
    });
  }

  Unused << SendClosed();
  Destroy();
  return IPC_OK();
}

void
StreamFilterParent::Destroy()
{
  // Close the channel asynchronously so the actor is never destroyed before
  // this message is fully processed.
  ActorThread()->Dispatch(
    NewRunnableMethod("StreamFilterParent::Close",
                      this,
                      &StreamFilterParent::Close),
    NS_DISPATCH_NORMAL);
}

IPCResult
StreamFilterParent::RecvSuspend()
{
  AssertIsActorThread();

  if (mState == State::TransferringData) {
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      self->mChannel->Suspend();

      RunOnActorThread(FUNC, [=] {
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
  AssertIsActorThread();

  if (mState == State::Suspended) {
    // Change state before resuming so incoming data is handled correctly
    // immediately after resuming.
    mState = State::TransferringData;

    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      self->mChannel->Resume();

      RunOnActorThread(FUNC, [=] {
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
  AssertIsActorThread();

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
  AssertIsActorThread();

  MOZ_ASSERT(mState == State::Disconnecting);

  Destroy();

  RefPtr<StreamFilterParent> self(this);
  RunOnIOThread(FUNC, [=] {
    self->FlushBufferedData();

    RunOnActorThread(FUNC, [=] {
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
  AssertIsActorThread();


  RunOnIOThread(
    NewRunnableMethod<Data&&>("StreamFilterParent::WriteMove",
                              this,
                              &StreamFilterParent::WriteMove,
                              Move(aData)));
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

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      reinterpret_cast<char*>(aData.Elements()),
                                      aData.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mOrigListener->OnDataAvailable(mChannel, mContext, stream,
                                      mOffset, aData.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  mOffset += aData.Length();
  return NS_OK;
}

/*****************************************************************************
 * nsIStreamListener
 *****************************************************************************/

NS_IMETHODIMP
StreamFilterParent::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  AssertIsMainThread();

  mContext = aContext;

  if (mState != State::Disconnected) {
    RefPtr<StreamFilterParent> self(this);
    RunOnActorThread(FUNC, [=] {
      if (self->IPCActive()) {
        self->mState = State::TransferringData;
        self->CheckResult(self->SendStartRequest());
      }
    });
  }

  nsresult rv = mOrigListener->OnStartRequest(aRequest, aContext);

  // Important: Do this only *after* running the next listener in the chain, so
  // that we get the final delivery target after any retargeting that it may do.
  if (nsCOMPtr<nsIThreadRetargetableRequest> req = do_QueryInterface(aRequest)) {
    nsCOMPtr<nsIEventTarget> thread;
    Unused << req->GetDeliveryTarget(getter_AddRefs(thread));
    if (thread) {
      mIOThread = Move(thread);
    }
  }

  return rv;
}

NS_IMETHODIMP
StreamFilterParent::OnStopRequest(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsresult aStatusCode)
{
  AssertIsMainThread();

  mReceivedStop = true;
  if (mState == State::Disconnected) {
    return EmitStopRequest(aStatusCode);
  }

  RefPtr<StreamFilterParent> self(this);
  RunOnActorThread(FUNC, [=] {
    if (self->IPCActive()) {
      self->CheckResult(self->SendStopRequest(aStatusCode));
    }
  });
  return NS_OK;
}

nsresult
StreamFilterParent::EmitStopRequest(nsresult aStatusCode)
{
  AssertIsMainThread();
  MOZ_ASSERT(!mSentStop);

  mSentStop = true;
  return mOrigListener->OnStopRequest(mChannel, mContext, aStatusCode);
}

/*****************************************************************************
 * Incoming data handling
 *****************************************************************************/

void
StreamFilterParent::DoSendData(Data&& aData)
{
  AssertIsActorThread();

  if (mState == State::TransferringData) {
    CheckResult(SendData(aData));
  }
}

NS_IMETHODIMP
StreamFilterParent::OnDataAvailable(nsIRequest* aRequest,
                                    nsISupports* aContext,
                                    nsIInputStream* aInputStream,
                                    uint64_t aOffset,
                                    uint32_t aCount)
{
  AssertIsIOThread();

  if (mState == State::Disconnected) {
    // If we're offloading data in a thread pool, it's possible that we'll
    // have buffered some additional data while waiting for the buffer to
    // flush. So, if there's any buffered data left, flush that before we
    // flush this incoming data.
    //
    // Note: When in the eDisconnected state, the buffer list is guaranteed
    // never to be accessed by another thread during an OnDataAvailable call.
    if (!mBufferedData.isEmpty()) {
      FlushBufferedData();
    }

    mOffset += aCount;
    return mOrigListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                          mOffset - aCount, aCount);
  }

  Data data;
  data.SetLength(aCount);

  uint32_t count;
  nsresult rv = aInputStream->Read(reinterpret_cast<char*>(data.Elements()),
                                   aCount, &count);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(count == aCount, NS_ERROR_UNEXPECTED);

  if (mState == State::Disconnecting) {
    MutexAutoLock al(mBufferMutex);
    BufferData(Move(data));
  } else if (mState == State::Closed) {
    return NS_ERROR_FAILURE;
  } else {
    ActorThread()->Dispatch(
      NewRunnableMethod<Data&&>("StreamFilterParent::DoSendData",
                                this,
                                &StreamFilterParent::DoSendData,
                                Move(data)),
      NS_DISPATCH_NORMAL);
  }
  return NS_OK;
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
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] {
      if (!mSentStop) {
        nsresult rv = self->EmitStopRequest(NS_OK);
        Unused << NS_WARN_IF(NS_FAILED(rv));
      }
    });
  }

  return NS_OK;
}

/*****************************************************************************
 * Thread helpers
 *****************************************************************************/

nsIEventTarget*
StreamFilterParent::ActorThread()
{
  return gSocketTransportService;
}

bool
StreamFilterParent::IsActorThread()
{
  return ActorThread()->IsOnCurrentThread();
}

void
StreamFilterParent::AssertIsActorThread()
{
  MOZ_ASSERT(IsActorThread());
}

nsIEventTarget*
StreamFilterParent::IOThread()
{
  return mIOThread;
}

bool
StreamFilterParent::IsIOThread()
{
  return mIOThread->IsOnCurrentThread();
}

void
StreamFilterParent::AssertIsIOThread()
{
  MOZ_ASSERT(IsIOThread());
}

template<typename Function>
void
StreamFilterParent::RunOnMainThread(const char* aName, Function&& aFunc)
{
  mQueue->RunOrEnqueue(new ChannelEventFunction(mMainThread, Move(aFunc)));
}

void
StreamFilterParent::RunOnMainThread(already_AddRefed<Runnable> aRunnable)
{
  mQueue->RunOrEnqueue(new ChannelEventRunnable(mMainThread, Move(aRunnable)));
}

template<typename Function>
void
StreamFilterParent::RunOnIOThread(const char* aName, Function&& aFunc)
{
  mQueue->RunOrEnqueue(new ChannelEventFunction(mIOThread, Move(aFunc)));
}

void
StreamFilterParent::RunOnIOThread(already_AddRefed<Runnable> aRunnable)
{
  mQueue->RunOrEnqueue(new ChannelEventRunnable(mIOThread, Move(aRunnable)));
}

template<typename Function>
void
StreamFilterParent::RunOnActorThread(const char* aName, Function&& aFunc)
{
  // We don't use mQueue for dispatch to the actor thread.
  //
  // The main thread and IO thread are used for dispatching events to the
  // wrapped stream listener, and those events need to be processed
  // consistently, in the order they were dispatched. An event dispatched to the
  // main thread can't be run before events that were dispatched to the IO
  // thread before it.
  //
  // Additionally, the IO thread is likely to be a thread pool, which means that
  // without thread-safe queuing, it's possible for multiple events dispatched
  // to it to be processed in parallel, or out of order.
  //
  // The actor thread, however, is always a serial event target. Its events are
  // always processed in order, and events dispatched to the actor thread are
  // independent of the events in the output event queue.
  if (IsActorThread()) {
    aFunc();
  } else {
    ActorThread()->Dispatch(
      Move(NS_NewRunnableFunction(aName, aFunc)),
      NS_DISPATCH_NORMAL);
  }
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

void
StreamFilterParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsActorThread();

  if (mState != State::Disconnected && mState != State::Closed) {
    Broken();
  }
}

void
StreamFilterParent::DeallocPStreamFilterParent()
{
  RefPtr<StreamFilterParent> self = dont_AddRef(this);
}

NS_IMPL_ISUPPORTS(StreamFilterParent, nsIStreamListener, nsIRequestObserver, nsIThreadRetargetableStreamListener)

} // namespace extensions
} // namespace mozilla

