/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIInputStream.h"
#include "nsITraceableChannel.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSocketTransportService2.h"
#include "nsStringStream.h"
#include "mozilla/net/DocumentChannelChild.h"

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

class ChannelEventWrapper : public ChannelEvent {
 public:
  ChannelEventWrapper(nsIEventTarget* aTarget) : mTarget(aTarget) {}

  already_AddRefed<nsIEventTarget> GetEventTarget() override {
    return do_AddRef(mTarget);
  }

 protected:
  ~ChannelEventWrapper() override = default;

 private:
  nsCOMPtr<nsIEventTarget> mTarget;
};

class ChannelEventFunction final : public ChannelEventWrapper {
 public:
  ChannelEventFunction(nsIEventTarget* aTarget, std::function<void()>&& aFunc)
      : ChannelEventWrapper(aTarget), mFunc(std::move(aFunc)) {}

  void Run() override { mFunc(); }

 protected:
  ~ChannelEventFunction() override = default;

 private:
  std::function<void()> mFunc;
};

class ChannelEventRunnable final : public ChannelEventWrapper {
 public:
  ChannelEventRunnable(nsIEventTarget* aTarget,
                       already_AddRefed<Runnable> aRunnable)
      : ChannelEventWrapper(aTarget), mRunnable(aRunnable) {}

  void Run() override {
    nsresult rv = mRunnable->Run();
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

 protected:
  ~ChannelEventRunnable() override = default;

 private:
  RefPtr<Runnable> mRunnable;
};

}  // anonymous namespace

/*****************************************************************************
 * Initialization
 *****************************************************************************/

StreamFilterParent::StreamFilterParent()
    : mMainThread(GetCurrentThreadEventTarget()),
      mIOThread(mMainThread),
      mQueue(new ChannelEventQueue(static_cast<nsIStreamListener*>(this))),
      mBufferMutex("StreamFilter buffer mutex"),
      mReceivedStop(false),
      mSentStop(false),
      mContext(nullptr),
      mOffset(0),
      mState(State::Uninitialized) {}

StreamFilterParent::~StreamFilterParent() {
  NS_ReleaseOnMainThread("StreamFilterParent::mChannel", mChannel.forget());
  NS_ReleaseOnMainThread("StreamFilterParent::mLoadGroup", mLoadGroup.forget());
  NS_ReleaseOnMainThread("StreamFilterParent::mOrigListener",
                         mOrigListener.forget());
  NS_ReleaseOnMainThread("StreamFilterParent::mContext", mContext.forget());
}

auto StreamFilterParent::Create(dom::ContentParent* aContentParent,
                                uint64_t aChannelId, const nsAString& aAddonId)
    -> RefPtr<ChildEndpointPromise> {
  AssertIsMainThread();

  auto& webreq = WebRequestService::GetSingleton();

  RefPtr<nsAtom> addonId = NS_Atomize(aAddonId);
  nsCOMPtr<nsITraceableChannel> channel =
      webreq.GetTraceableChannel(aChannelId, addonId, aContentParent);

  RefPtr<mozilla::net::nsHttpChannel> chan = do_QueryObject(channel);
  if (!chan) {
    return ChildEndpointPromise::CreateAndReject(false, __func__);
  }

  return chan->AttachStreamFilter(aContentParent ? aContentParent->OtherPid()
                                                 : base::GetCurrentProcId());
}

/* static */
void StreamFilterParent::Attach(nsIChannel* aChannel,
                                ParentEndpoint&& aEndpoint) {
  auto self = MakeRefPtr<StreamFilterParent>();

  self->ActorThread()->Dispatch(
      NewRunnableMethod<ParentEndpoint&&>("StreamFilterParent::Bind", self,
                                          &StreamFilterParent::Bind,
                                          std::move(aEndpoint)),
      NS_DISPATCH_NORMAL);

  self->Init(aChannel);

  // IPC owns this reference now.
  Unused << self.forget();
}

void StreamFilterParent::Bind(ParentEndpoint&& aEndpoint) {
  aEndpoint.Bind(this);
}

void StreamFilterParent::Init(nsIChannel* aChannel) {
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
StreamFilterParent::CheckListenerChain() {
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

void StreamFilterParent::Broken() {
  AssertIsActorThread();

  switch (mState) {
    case State::Initialized:
    case State::TransferringData:
    case State::Suspended: {
      mState = State::Disconnecting;
      RefPtr<StreamFilterParent> self(this);
      RunOnMainThread(FUNC, [=] {
        if (self->mChannel) {
          self->mChannel->Cancel(NS_ERROR_FAILURE);
        }
      });

      FinishDisconnect();
    } break;

    default:
      break;
  }
}

/*****************************************************************************
 * State change requests
 *****************************************************************************/

IPCResult StreamFilterParent::RecvClose() {
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

void StreamFilterParent::Destroy() {
  // Close the channel asynchronously so the actor is never destroyed before
  // this message is fully processed.
  ActorThread()->Dispatch(NewRunnableMethod("StreamFilterParent::Close", this,
                                            &StreamFilterParent::Close),
                          NS_DISPATCH_NORMAL);
}

IPCResult StreamFilterParent::RecvDestroy() {
  AssertIsActorThread();
  Destroy();
  return IPC_OK();
}

IPCResult StreamFilterParent::RecvSuspend() {
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

IPCResult StreamFilterParent::RecvResume() {
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
IPCResult StreamFilterParent::RecvDisconnect() {
  AssertIsActorThread();

  if (mState == State::Suspended) {
    RefPtr<StreamFilterParent> self(this);
    RunOnMainThread(FUNC, [=] { self->mChannel->Resume(); });
  } else if (mState != State::TransferringData) {
    return IPC_OK();
  }

  mState = State::Disconnecting;
  CheckResult(SendFlushData());
  return IPC_OK();
}

IPCResult StreamFilterParent::RecvFlushedData() {
  AssertIsActorThread();

  MOZ_ASSERT(mState == State::Disconnecting);

  Destroy();

  FinishDisconnect();
  return IPC_OK();
}

void StreamFilterParent::FinishDisconnect() {
  RefPtr<StreamFilterParent> self(this);
  RunOnIOThread(FUNC, [=] {
    self->FlushBufferedData();

    RunOnMainThread(FUNC, [=] {
      if (self->mLoadGroup) {
        Unused << self->mLoadGroup->RemoveRequest(self, nullptr, NS_OK);
      }
    });

    RunOnActorThread(FUNC, [=] {
      if (self->mState != State::Closed) {
        self->mState = State::Disconnected;
        self->mDisconnected = true;
      }
    });
  });
}

/*****************************************************************************
 * Data output
 *****************************************************************************/

IPCResult StreamFilterParent::RecvWrite(Data&& aData) {
  AssertIsActorThread();

  RunOnIOThread(NewRunnableMethod<Data&&>("StreamFilterParent::WriteMove", this,
                                          &StreamFilterParent::WriteMove,
                                          std::move(aData)));
  return IPC_OK();
}

void StreamFilterParent::WriteMove(Data&& aData) {
  nsresult rv = Write(aData);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

nsresult StreamFilterParent::Write(Data& aData) {
  AssertIsIOThread();

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(
      getter_AddRefs(stream),
      MakeSpan(reinterpret_cast<char*>(aData.Elements()), aData.Length()),
      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  rv =
      mOrigListener->OnDataAvailable(mChannel, stream, mOffset, aData.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  mOffset += aData.Length();
  return NS_OK;
}

/*****************************************************************************
 * nsIRequest
 *****************************************************************************/

NS_IMETHODIMP
StreamFilterParent::GetName(nsACString& aName) {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->GetName(aName);
}

NS_IMETHODIMP
StreamFilterParent::GetStatus(nsresult* aStatus) {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->GetStatus(aStatus);
}

NS_IMETHODIMP
StreamFilterParent::IsPending(bool* aIsPending) {
  switch (mState) {
    case State::Initialized:
    case State::TransferringData:
    case State::Suspended:
      *aIsPending = true;
      break;
    default:
      *aIsPending = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
StreamFilterParent::Cancel(nsresult aResult) {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->Cancel(aResult);
}

NS_IMETHODIMP
StreamFilterParent::Suspend() {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->Suspend();
}

NS_IMETHODIMP
StreamFilterParent::Resume() {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->Resume();
}

NS_IMETHODIMP
StreamFilterParent::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = mLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
StreamFilterParent::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
StreamFilterParent::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  MOZ_TRY(mChannel->GetLoadFlags(aLoadFlags));
  *aLoadFlags &= ~nsIChannel::LOAD_DOCUMENT_URI;
  return NS_OK;
}

NS_IMETHODIMP
StreamFilterParent::SetLoadFlags(nsLoadFlags aLoadFlags) {
  AssertIsMainThread();
  MOZ_ASSERT(mChannel);
  return mChannel->SetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
StreamFilterParent::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
StreamFilterParent::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

/*****************************************************************************
 * nsIStreamListener
 *****************************************************************************/

NS_IMETHODIMP
StreamFilterParent::OnStartRequest(nsIRequest* aRequest) {
  AssertIsMainThread();

  // If a StreamFilter's request results in an external redirect, that
  // StreamFilter should not monitor the redirected request's response (although
  // an identical StreamFilter may be created for it). A StreamFilter should,
  // however, monitor the response of an "internal" redirect (i.e. a
  // browser-initiated rather than server-initiated redirect); in this case,
  // mChannel must be replaced.
  if (aRequest != mChannel) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    nsCOMPtr<nsILoadInfo> loadInfo = channel ? channel->LoadInfo() : nullptr;

    if (loadInfo && loadInfo->RedirectChain().IsEmpty()) {
      MOZ_DIAGNOSTIC_ASSERT(
          !loadInfo->RedirectChainIncludingInternalRedirects().IsEmpty(),
          "We should be performing an internal redirect.");
      mChannel = channel;
    } else {
      mDisconnected = true;

      RefPtr<StreamFilterParent> self(this);
      RunOnActorThread(FUNC, [=] {
        if (self->IPCActive()) {
          self->mState = State::Disconnected;
          CheckResult(
              self->SendError(NS_LITERAL_CSTRING("Channel redirected")));
        }
      });
    }
  }

  if (!mDisconnected) {
    Unused << mChannel->GetLoadGroup(getter_AddRefs(mLoadGroup));
    if (mLoadGroup) {
      Unused << mLoadGroup->AddRequest(this, nullptr);
    }
  }

  nsresult rv = mOrigListener->OnStartRequest(aRequest);

  // Important: Do this only *after* running the next listener in the chain, so
  // that we get the final delivery target after any retargeting that it may do.
  if (nsCOMPtr<nsIThreadRetargetableRequest> req =
          do_QueryInterface(aRequest)) {
    nsCOMPtr<nsIEventTarget> thread;
    Unused << req->GetDeliveryTarget(getter_AddRefs(thread));
    if (thread) {
      mIOThread = std::move(thread);
    }
  }

  // Important: Do this *after* we have set the thread delivery target, or it is
  // possible in rare circumstances for an extension to attempt to write data
  // before the thread has been set up, even though there are several layers of
  // asynchrony involved.
  if (!mDisconnected) {
    RefPtr<StreamFilterParent> self(this);
    RunOnActorThread(FUNC, [=] {
      if (self->IPCActive()) {
        self->mState = State::TransferringData;
        self->CheckResult(self->SendStartRequest());
      }
    });
  }

  return rv;
}

NS_IMETHODIMP
StreamFilterParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  AssertIsMainThread();

  mReceivedStop = true;
  if (mDisconnected) {
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

nsresult StreamFilterParent::EmitStopRequest(nsresult aStatusCode) {
  AssertIsMainThread();
  MOZ_ASSERT(!mSentStop);

  mSentStop = true;
  nsresult rv = mOrigListener->OnStopRequest(mChannel, aStatusCode);

  if (mLoadGroup && !mDisconnected) {
    Unused << mLoadGroup->RemoveRequest(this, nullptr, aStatusCode);
  }

  return rv;
}

/*****************************************************************************
 * Incoming data handling
 *****************************************************************************/

void StreamFilterParent::DoSendData(Data&& aData) {
  AssertIsActorThread();

  if (mState == State::TransferringData) {
    CheckResult(SendData(aData));
  }
}

NS_IMETHODIMP
StreamFilterParent::OnDataAvailable(nsIRequest* aRequest,
                                    nsIInputStream* aInputStream,
                                    uint64_t aOffset, uint32_t aCount) {
  AssertIsIOThread();

  if (mDisconnected) {
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
    return mOrigListener->OnDataAvailable(aRequest, aInputStream,
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
    BufferData(std::move(data));
  } else if (mState == State::Closed) {
    return NS_ERROR_FAILURE;
  } else {
    ActorThread()->Dispatch(
        NewRunnableMethod<Data&&>("StreamFilterParent::DoSendData", this,
                                  &StreamFilterParent::DoSendData,
                                  std::move(data)),
        NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

nsresult StreamFilterParent::FlushBufferedData() {
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

nsIEventTarget* StreamFilterParent::ActorThread() {
  return net::gSocketTransportService;
}

bool StreamFilterParent::IsActorThread() {
  return ActorThread()->IsOnCurrentThread();
}

void StreamFilterParent::AssertIsActorThread() { MOZ_ASSERT(IsActorThread()); }

nsIEventTarget* StreamFilterParent::IOThread() { return mIOThread; }

bool StreamFilterParent::IsIOThread() { return mIOThread->IsOnCurrentThread(); }

void StreamFilterParent::AssertIsIOThread() { MOZ_ASSERT(IsIOThread()); }

template <typename Function>
void StreamFilterParent::RunOnMainThread(const char* aName, Function&& aFunc) {
  mQueue->RunOrEnqueue(new ChannelEventFunction(mMainThread, std::move(aFunc)));
}

void StreamFilterParent::RunOnMainThread(already_AddRefed<Runnable> aRunnable) {
  mQueue->RunOrEnqueue(
      new ChannelEventRunnable(mMainThread, std::move(aRunnable)));
}

template <typename Function>
void StreamFilterParent::RunOnIOThread(const char* aName, Function&& aFunc) {
  mQueue->RunOrEnqueue(new ChannelEventFunction(mIOThread, std::move(aFunc)));
}

void StreamFilterParent::RunOnIOThread(already_AddRefed<Runnable> aRunnable) {
  mQueue->RunOrEnqueue(
      new ChannelEventRunnable(mIOThread, std::move(aRunnable)));
}

template <typename Function>
void StreamFilterParent::RunOnActorThread(const char* aName, Function&& aFunc) {
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
    ActorThread()->Dispatch(std::move(NS_NewRunnableFunction(aName, aFunc)),
                            NS_DISPATCH_NORMAL);
  }
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

void StreamFilterParent::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsActorThread();

  if (mState != State::Disconnected && mState != State::Closed) {
    Broken();
  }
}

void StreamFilterParent::ActorDealloc() {
  RefPtr<StreamFilterParent> self = dont_AddRef(this);
}

NS_INTERFACE_MAP_BEGIN(StreamFilterParent)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(StreamFilterParent)
NS_IMPL_RELEASE(StreamFilterParent)

}  // namespace extensions
}  // namespace mozilla
