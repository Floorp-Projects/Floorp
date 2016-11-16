/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "base/compiler_specific.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/NeckoChild.h"
#include "WebSocketChannelChild.h"
#include "nsITabChild.h"
#include "nsNetUtil.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "SerializedLoadContext.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(WebSocketChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) WebSocketChannelChild::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "WebSocketChannelChild");

  if (mRefCnt == 1) {
    MaybeReleaseIPCObject();
    return mRefCnt;
  }

  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_INTERFACE_MAP_BEGIN(WebSocketChannelChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketChannel)
  NS_INTERFACE_MAP_ENTRY(nsIProtocolHandler)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketChannel)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableRequest)
NS_INTERFACE_MAP_END

WebSocketChannelChild::WebSocketChannelChild(bool aEncrypted)
 : mIPCState(Closed)
 , mMutex("WebSocketChannelChild::mMutex")
{
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannelChild::WebSocketChannelChild() %p\n", this));
  mEncrypted = aEncrypted;
  mEventQ = new ChannelEventQueue(static_cast<nsIWebSocketChannel*>(this));
}

WebSocketChannelChild::~WebSocketChannelChild()
{
  LOG(("WebSocketChannelChild::~WebSocketChannelChild() %p\n", this));
}

void
WebSocketChannelChild::AddIPDLReference()
{
  MOZ_ASSERT(NS_IsMainThread());

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mIPCState == Closed, "Attempt to retain more than one IPDL reference");
    mIPCState = Opened;
  }

  AddRef();
}

void
WebSocketChannelChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(NS_IsMainThread());

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mIPCState != Closed, "Attempt to release nonexistent IPDL reference");
    mIPCState = Closed;
  }

  Release();
}

void
WebSocketChannelChild::MaybeReleaseIPCObject()
{
  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return;
    }

    mIPCState = Closing;
  }

  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchToMainThread(NewRunnableMethod(this,
                                                &WebSocketChannelChild::MaybeReleaseIPCObject)));
    return;
  }

  SendDeleteSelf();
}

void
WebSocketChannelChild::GetEffectiveURL(nsAString& aEffectiveURL) const
{
  aEffectiveURL = mEffectiveURL;
}

bool
WebSocketChannelChild::IsEncrypted() const
{
  return mEncrypted;
}

class WrappedChannelEvent : public Runnable
{
public:
  explicit WrappedChannelEvent(ChannelEvent *aChannelEvent)
    : mChannelEvent(aChannelEvent)
  {
    MOZ_RELEASE_ASSERT(aChannelEvent);
  }
  NS_IMETHOD Run() override
  {
    mChannelEvent->Run();
    return NS_OK;
  }
private:
  nsAutoPtr<ChannelEvent> mChannelEvent;
};

void
WebSocketChannelChild::DispatchToTargetThread(ChannelEvent *aChannelEvent)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mTargetThread);
  MOZ_RELEASE_ASSERT(aChannelEvent);

  mTargetThread->Dispatch(new WrappedChannelEvent(aChannelEvent),
                          NS_DISPATCH_NORMAL);
}

class EventTargetDispatcher : public ChannelEvent
{
public:
  EventTargetDispatcher(ChannelEvent* aChannelEvent,
                        nsIEventTarget* aEventTarget)
    : mChannelEvent(aChannelEvent)
    , mEventTarget(aEventTarget)
  {}

  void Run()
  {
    if (mEventTarget) {
      mEventTarget->Dispatch(new WrappedChannelEvent(mChannelEvent.forget()),
                             NS_DISPATCH_NORMAL);
      return;
    }

    mChannelEvent->Run();
  }

private:
  nsAutoPtr<ChannelEvent> mChannelEvent;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

class StartEvent : public ChannelEvent
{
 public:
  StartEvent(WebSocketChannelChild* aChild,
             const nsCString& aProtocol,
             const nsCString& aExtensions,
             const nsString& aEffectiveURL,
             bool aEncrypted)
  : mChild(aChild)
  , mProtocol(aProtocol)
  , mExtensions(aExtensions)
  , mEffectiveURL(aEffectiveURL)
  , mEncrypted(aEncrypted)
  {}

  void Run()
  {
    mChild->OnStart(mProtocol, mExtensions, mEffectiveURL, mEncrypted);
  }
 private:
  RefPtr<WebSocketChannelChild> mChild;
  nsCString mProtocol;
  nsCString mExtensions;
  nsString mEffectiveURL;
  bool mEncrypted;
};

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnStart(const nsCString& aProtocol,
                                   const nsCString& aExtensions,
                                   const nsString& aEffectiveURL,
                                   const bool& aEncrypted)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new StartEvent(this, aProtocol, aExtensions,
                                             aEffectiveURL, aEncrypted),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnStart(const nsCString& aProtocol,
                               const nsCString& aExtensions,
                               const nsString& aEffectiveURL,
                               const bool& aEncrypted)
{
  LOG(("WebSocketChannelChild::RecvOnStart() %p\n", this));
  SetProtocol(aProtocol);
  mNegotiatedExtensions = aExtensions;
  mEffectiveURL = aEffectiveURL;
  mEncrypted = aEncrypted;

  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnStart(mListenerMT->mContext);
  }
}

class StopEvent : public ChannelEvent
{
 public:
  StopEvent(WebSocketChannelChild* aChild,
            const nsresult& aStatusCode)
  : mChild(aChild)
  , mStatusCode(aStatusCode)
  {}

  void Run()
  {
    mChild->OnStop(mStatusCode);
  }
 private:
  RefPtr<WebSocketChannelChild> mChild;
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnStop(const nsresult& aStatusCode)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new StopEvent(this, aStatusCode),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnStop(const nsresult& aStatusCode)
{
  LOG(("WebSocketChannelChild::RecvOnStop() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnStop(mListenerMT->mContext, aStatusCode);
  }
}

class MessageEvent : public ChannelEvent
{
 public:
  MessageEvent(WebSocketChannelChild* aChild,
               const nsCString& aMessage,
               bool aBinary)
  : mChild(aChild)
  , mMessage(aMessage)
  , mBinary(aBinary)
  {}

  void Run()
  {
    if (!mBinary) {
      mChild->OnMessageAvailable(mMessage);
    } else {
      mChild->OnBinaryMessageAvailable(mMessage);
    }
  }
 private:
  RefPtr<WebSocketChannelChild> mChild;
  nsCString mMessage;
  bool mBinary;
};

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnMessageAvailable(const nsCString& aMsg)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new MessageEvent(this, aMsg, false),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnMessageAvailable(const nsCString& aMsg)
{
  LOG(("WebSocketChannelChild::RecvOnMessageAvailable() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnMessageAvailable(mListenerMT->mContext, aMsg);
  }
}

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnBinaryMessageAvailable(const nsCString& aMsg)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new MessageEvent(this, aMsg, true),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnBinaryMessageAvailable(const nsCString& aMsg)
{
  LOG(("WebSocketChannelChild::RecvOnBinaryMessageAvailable() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnBinaryMessageAvailable(mListenerMT->mContext,
                                                     aMsg);
  }
}

class AcknowledgeEvent : public ChannelEvent
{
 public:
  AcknowledgeEvent(WebSocketChannelChild* aChild,
                   const uint32_t& aSize)
  : mChild(aChild)
  , mSize(aSize)
  {}

  void Run()
  {
    mChild->OnAcknowledge(mSize);
  }
 private:
  RefPtr<WebSocketChannelChild> mChild;
  uint32_t mSize;
};

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnAcknowledge(const uint32_t& aSize)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new AcknowledgeEvent(this, aSize),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnAcknowledge(const uint32_t& aSize)
{
  LOG(("WebSocketChannelChild::RecvOnAcknowledge() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnAcknowledge(mListenerMT->mContext, aSize);
  }
}

class ServerCloseEvent : public ChannelEvent
{
 public:
  ServerCloseEvent(WebSocketChannelChild* aChild,
                   const uint16_t aCode,
                   const nsCString &aReason)
  : mChild(aChild)
  , mCode(aCode)
  , mReason(aReason)
  {}

  void Run()
  {
    mChild->OnServerClose(mCode, mReason);
  }
 private:
  RefPtr<WebSocketChannelChild> mChild;
  uint16_t mCode;
  nsCString mReason;
};

mozilla::ipc::IPCResult
WebSocketChannelChild::RecvOnServerClose(const uint16_t& aCode,
                                         const nsCString& aReason)
{
  mEventQ->RunOrEnqueue(
    new EventTargetDispatcher(new ServerCloseEvent(this, aCode, aReason),
                              mTargetThread));

  return IPC_OK();
}

void
WebSocketChannelChild::OnServerClose(const uint16_t& aCode,
                                     const nsCString& aReason)
{
  LOG(("WebSocketChannelChild::RecvOnServerClose() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    mListenerMT->mListener->OnServerClose(mListenerMT->mContext, aCode,
                                          aReason);
  }
}

NS_IMETHODIMP
WebSocketChannelChild::AsyncOpen(nsIURI *aURI,
                                 const nsACString &aOrigin,
                                 uint64_t aInnerWindowID,
                                 nsIWebSocketListener *aListener,
                                 nsISupports *aContext)
{
  LOG(("WebSocketChannelChild::AsyncOpen() %p\n", this));

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  MOZ_ASSERT((aURI && !mIsServerSide) || (!aURI && mIsServerSide),
             "Invalid aURI for WebSocketChannelChild::AsyncOpen");
  MOZ_ASSERT(aListener && !mListenerMT,
             "Invalid state for WebSocketChannelChild::AsyncOpen");

  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsITabChild),
                                getter_AddRefs(iTabChild));
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }
  if (MissingRequiredTabChild(tabChild, "websocket")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Corresponding release in DeallocPWebSocket
  AddIPDLReference();

  OptionalURIParams uri;
  OptionalLoadInfoArgs loadInfoArgs;
  OptionalTransportProvider transportProvider;

  if (!mIsServerSide) {
    uri = URIParams();
    SerializeURI(aURI, uri.get_URIParams());
    nsresult rv = LoadInfoToLoadInfoArgs(mLoadInfo, &loadInfoArgs);
    NS_ENSURE_SUCCESS(rv, rv);

    transportProvider = void_t();
  } else {
    uri = void_t();
    loadInfoArgs = void_t();

    MOZ_ASSERT(mServerTransportProvider);
    PTransportProviderChild *ipcChild;
    nsresult rv = mServerTransportProvider->GetIPCChild(&ipcChild);
    NS_ENSURE_SUCCESS(rv, rv);

    transportProvider = ipcChild;
  }

  gNeckoChild->SendPWebSocketConstructor(this, tabChild,
                                         IPC::SerializedLoadContext(this),
                                         mSerial);
  if (!SendAsyncOpen(uri, nsCString(aOrigin), aInnerWindowID, mProtocol,
                     mEncrypted, mPingInterval, mClientSetPingInterval,
                     mPingResponseTimeout, mClientSetPingTimeout, loadInfoArgs,
                     transportProvider, mNegotiatedExtensions)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mIsServerSide) {
    mServerTransportProvider = nullptr;
  }

  mOriginalURI = aURI;
  mURI = mOriginalURI;
  mListenerMT = new ListenerAndContextContainer(aListener, aContext);
  mOrigin = aOrigin;
  mWasOpened = 1;

  return NS_OK;
}

class CloseEvent : public Runnable
{
public:
  CloseEvent(WebSocketChannelChild *aChild,
             uint16_t aCode,
             const nsACString& aReason)
    : mChild(aChild)
    , mCode(aCode)
    , mReason(aReason)
  {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mChild->Close(mCode, mReason);
    return NS_OK;
  }
private:
  RefPtr<WebSocketChannelChild> mChild;
  uint16_t                        mCode;
  nsCString                       mReason;
};

NS_IMETHODIMP
WebSocketChannelChild::Close(uint16_t code, const nsACString & reason)
{
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(NS_GetCurrentThread() == mTargetThread);
    return NS_DispatchToMainThread(new CloseEvent(this, code, reason));
  }
  LOG(("WebSocketChannelChild::Close() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendClose(code, nsCString(reason))) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class MsgEvent : public Runnable
{
public:
  MsgEvent(WebSocketChannelChild *aChild,
           const nsACString &aMsg,
           bool aBinaryMsg)
    : mChild(aChild)
    , mMsg(aMsg)
    , mBinaryMsg(aBinaryMsg)
  {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    if (mBinaryMsg) {
      mChild->SendBinaryMsg(mMsg);
    } else {
      mChild->SendMsg(mMsg);
    }
    return NS_OK;
  }
private:
  RefPtr<WebSocketChannelChild> mChild;
  nsCString                       mMsg;
  bool                            mBinaryMsg;
};

NS_IMETHODIMP
WebSocketChannelChild::SendMsg(const nsACString &aMsg)
{
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    return NS_DispatchToMainThread(new MsgEvent(this, aMsg, false));
  }
  LOG(("WebSocketChannelChild::SendMsg() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendMsg(nsCString(aMsg))) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelChild::SendBinaryMsg(const nsACString &aMsg)
{
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    return NS_DispatchToMainThread(new MsgEvent(this, aMsg, true));
  }
  LOG(("WebSocketChannelChild::SendBinaryMsg() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendBinaryMsg(nsCString(aMsg))) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class BinaryStreamEvent : public Runnable
{
public:
  BinaryStreamEvent(WebSocketChannelChild *aChild,
                    OptionalInputStreamParams *aStream,
                    uint32_t aLength)
    : mChild(aChild)
    , mStream(aStream)
    , mLength(aLength)
  {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mChild->SendBinaryStream(mStream, mLength);
    return NS_OK;
  }
private:
  RefPtr<WebSocketChannelChild>      mChild;
  nsAutoPtr<OptionalInputStreamParams> mStream;
  uint32_t                             mLength;
};

NS_IMETHODIMP
WebSocketChannelChild::SendBinaryStream(nsIInputStream *aStream,
                                        uint32_t aLength)
{
  OptionalInputStreamParams *stream = new OptionalInputStreamParams();
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(aStream, *stream, fds);

  MOZ_ASSERT(fds.IsEmpty());

  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(NS_GetCurrentThread() == mTargetThread);
    return NS_DispatchToMainThread(new BinaryStreamEvent(this, stream, aLength));
  }
  return SendBinaryStream(stream, aLength);
}

nsresult
WebSocketChannelChild::SendBinaryStream(OptionalInputStreamParams *aStream,
                                        uint32_t aLength)
{
  LOG(("WebSocketChannelChild::SendBinaryStream() %p\n", this));

  nsAutoPtr<OptionalInputStreamParams> stream(aStream);

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendBinaryStream(*stream, aLength)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelChild::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG(("WebSocketChannelChild::GetSecurityInfo() %p\n", this));
  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------
// WebSocketChannelChild::nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketChannelChild::RetargetDeliveryTo(nsIEventTarget* aTargetThread)
{
  nsresult rv = BaseWebSocketChannel::RetargetDeliveryTo(aTargetThread);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  return mEventQ->RetargetDeliveryTo(aTargetThread);
}

bool
WebSocketChannelChild::IsOnTargetThread()
{
  MOZ_ASSERT(mTargetThread);
  bool isOnTargetThread = false;
  nsresult rv = mTargetThread->IsOnCurrentThread(&isOnTargetThread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_FAILED(rv) ? false : isOnTargetThread;
}

} // namespace net
} // namespace mozilla
