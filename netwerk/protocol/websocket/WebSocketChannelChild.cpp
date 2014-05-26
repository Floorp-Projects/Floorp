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
#include "mozilla/net/ChannelEventQueue.h"
#include "SerializedLoadContext.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(WebSocketChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) WebSocketChannelChild::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(WebSocketChannelChild);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "WebSocketChannelChild");

  if (mRefCnt == 1 && mIPCOpen) {
    SendDeleteSelf();
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

WebSocketChannelChild::WebSocketChannelChild(bool aSecure)
 : mIPCOpen(false)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannelChild::WebSocketChannelChild() %p\n", this));
  BaseWebSocketChannel::mEncrypted = aSecure;
  mEventQ = new ChannelEventQueue(static_cast<nsIWebSocketChannel*>(this));
}

WebSocketChannelChild::~WebSocketChannelChild()
{
  LOG(("WebSocketChannelChild::~WebSocketChannelChild() %p\n", this));
}

void
WebSocketChannelChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
WebSocketChannelChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

class WrappedChannelEvent : public nsRunnable
{
public:
  WrappedChannelEvent(ChannelEvent *aChannelEvent)
    : mChannelEvent(aChannelEvent)
  {
    MOZ_RELEASE_ASSERT(aChannelEvent);
  }
  NS_IMETHOD Run()
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

class StartEvent : public ChannelEvent
{
 public:
  StartEvent(WebSocketChannelChild* aChild,
             const nsCString& aProtocol,
             const nsCString& aExtensions)
  : mChild(aChild)
  , mProtocol(aProtocol)
  , mExtensions(aExtensions)
  {}

  void Run()
  {
    mChild->OnStart(mProtocol, mExtensions);
  }
 private:
  WebSocketChannelChild* mChild;
  nsCString mProtocol;
  nsCString mExtensions;
};

bool
WebSocketChannelChild::RecvOnStart(const nsCString& aProtocol,
                                   const nsCString& aExtensions)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StartEvent(this, aProtocol, aExtensions));
  } else if (mTargetThread) {
    DispatchToTargetThread(new StartEvent(this, aProtocol, aExtensions));
  } else {
    OnStart(aProtocol, aExtensions);
  }
  return true;
}

void
WebSocketChannelChild::OnStart(const nsCString& aProtocol,
                               const nsCString& aExtensions)
{
  LOG(("WebSocketChannelChild::RecvOnStart() %p\n", this));
  SetProtocol(aProtocol);
  mNegotiatedExtensions = aExtensions;

  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnStart(mContext);
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
  WebSocketChannelChild* mChild;
  nsresult mStatusCode;
};

bool
WebSocketChannelChild::RecvOnStop(const nsresult& aStatusCode)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StopEvent(this, aStatusCode));
  } else if (mTargetThread) {
    DispatchToTargetThread(new StopEvent(this, aStatusCode));
  } else {
    OnStop(aStatusCode);
  }
  return true;
}

void
WebSocketChannelChild::OnStop(const nsresult& aStatusCode)
{
  LOG(("WebSocketChannelChild::RecvOnStop() %p\n", this));
  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnStop(mContext, aStatusCode);
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
  WebSocketChannelChild* mChild;
  nsCString mMessage;
  bool mBinary;
};

bool
WebSocketChannelChild::RecvOnMessageAvailable(const nsCString& aMsg)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new MessageEvent(this, aMsg, false));
  } else if (mTargetThread) {
    DispatchToTargetThread(new MessageEvent(this, aMsg, false));
   } else {
    OnMessageAvailable(aMsg);
  }
  return true;
}

void
WebSocketChannelChild::OnMessageAvailable(const nsCString& aMsg)
{
  LOG(("WebSocketChannelChild::RecvOnMessageAvailable() %p\n", this));
  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnMessageAvailable(mContext, aMsg);
  }
}

bool
WebSocketChannelChild::RecvOnBinaryMessageAvailable(const nsCString& aMsg)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new MessageEvent(this, aMsg, true));
  } else if (mTargetThread) {
    DispatchToTargetThread(new MessageEvent(this, aMsg, true));
  } else {
    OnBinaryMessageAvailable(aMsg);
  }
  return true;
}

void
WebSocketChannelChild::OnBinaryMessageAvailable(const nsCString& aMsg)
{
  LOG(("WebSocketChannelChild::RecvOnBinaryMessageAvailable() %p\n", this));
  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnBinaryMessageAvailable(mContext, aMsg);
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
  WebSocketChannelChild* mChild;
  uint32_t mSize;
};

bool
WebSocketChannelChild::RecvOnAcknowledge(const uint32_t& aSize)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new AcknowledgeEvent(this, aSize));
  } else if (mTargetThread) {
    DispatchToTargetThread(new AcknowledgeEvent(this, aSize));
  } else {
    OnAcknowledge(aSize);
  }
  return true;
}

void
WebSocketChannelChild::OnAcknowledge(const uint32_t& aSize)
{
  LOG(("WebSocketChannelChild::RecvOnAcknowledge() %p\n", this));
  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnAcknowledge(mContext, aSize);
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
  WebSocketChannelChild* mChild;
  uint16_t               mCode;
  nsCString              mReason;
};

bool
WebSocketChannelChild::RecvOnServerClose(const uint16_t& aCode,
                                         const nsCString& aReason)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new ServerCloseEvent(this, aCode, aReason));
  } else if (mTargetThread) {
    DispatchToTargetThread(new ServerCloseEvent(this, aCode, aReason));
  } else {
    OnServerClose(aCode, aReason);
  }
  return true;
}

void
WebSocketChannelChild::OnServerClose(const uint16_t& aCode,
                                     const nsCString& aReason)
{
  LOG(("WebSocketChannelChild::RecvOnServerClose() %p\n", this));
  if (mListener) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);;
    mListener->OnServerClose(mContext, aCode, aReason);
  }
}

NS_IMETHODIMP
WebSocketChannelChild::AsyncOpen(nsIURI *aURI,
                                 const nsACString &aOrigin,
                                 nsIWebSocketListener *aListener,
                                 nsISupports *aContext)
{
  LOG(("WebSocketChannelChild::AsyncOpen() %p\n", this));

  NS_ABORT_IF_FALSE(NS_IsMainThread(), "not main thread");
  NS_ABORT_IF_FALSE(aURI && aListener && !mListener, 
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

  URIParams uri;
  SerializeURI(aURI, uri);

  // Corresponding release in DeallocPWebSocket
  AddIPDLReference();

  gNeckoChild->SendPWebSocketConstructor(this, tabChild,
                                         IPC::SerializedLoadContext(this));
  if (!SendAsyncOpen(uri, nsCString(aOrigin), mProtocol, mEncrypted,
                     mPingInterval, mClientSetPingInterval,
                     mPingResponseTimeout, mClientSetPingTimeout))
    return NS_ERROR_UNEXPECTED;

  mOriginalURI = aURI;
  mURI = mOriginalURI;
  mListener = aListener;
  mContext = aContext;
  mOrigin = aOrigin;
  mWasOpened = 1;

  return NS_OK;
}

class CloseEvent : public nsRunnable
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
  NS_IMETHOD Run()
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mChild->Close(mCode, mReason);
    return NS_OK;
  }
private:
  nsRefPtr<WebSocketChannelChild> mChild;
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

  if (!mIPCOpen || !SendClose(code, nsCString(reason)))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

class MsgEvent : public nsRunnable
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
  NS_IMETHOD Run()
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
  nsRefPtr<WebSocketChannelChild> mChild;
  nsCString                       mMsg;
  bool                            mBinaryMsg;
};

NS_IMETHODIMP
WebSocketChannelChild::SendMsg(const nsACString &aMsg)
{
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(NS_GetCurrentThread() == mTargetThread);
    return NS_DispatchToMainThread(new MsgEvent(this, aMsg, false));
  }
  LOG(("WebSocketChannelChild::SendMsg() %p\n", this));

  if (!mIPCOpen || !SendSendMsg(nsCString(aMsg)))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelChild::SendBinaryMsg(const nsACString &aMsg)
{
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(NS_GetCurrentThread() == mTargetThread);
    return NS_DispatchToMainThread(new MsgEvent(this, aMsg, true));
  }
  LOG(("WebSocketChannelChild::SendBinaryMsg() %p\n", this));

  if (!mIPCOpen || !SendSendBinaryMsg(nsCString(aMsg)))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

class BinaryStreamEvent : public nsRunnable
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
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mChild->SendBinaryStream(mStream, mLength);
    return NS_OK;
  }
private:
  nsRefPtr<WebSocketChannelChild>      mChild;
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

  if (!mIPCOpen || !SendSendBinaryStream(*stream, aLength))
    return NS_ERROR_UNEXPECTED;
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

} // namespace net
} // namespace mozilla
