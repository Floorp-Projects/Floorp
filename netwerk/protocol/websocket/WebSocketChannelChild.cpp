/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "base/compiler_specific.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/net/NeckoChild.h"
#include "WebSocketChannelChild.h"
#include "nsContentUtils.h"
#include "nsIBrowserChild.h"
#include "nsNetUtil.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "SerializedLoadContext.h"
#include "mozilla/dom/ContentChild.h"
#include "nsITransportProvider.h"

using namespace mozilla::ipc;
using mozilla::dom::ContentChild;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(WebSocketChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) WebSocketChannelChild::Release() {
  MOZ_ASSERT(0 != mRefCnt, "dup release");
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
    : NeckoTargetHolder(nullptr),
      mIPCState(Closed),
      mMutex("WebSocketChannelChild::mMutex") {
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  LOG(("WebSocketChannelChild::WebSocketChannelChild() %p\n", this));
  mEncrypted = aEncrypted;
  mEventQ = new ChannelEventQueue(static_cast<nsIWebSocketChannel*>(this));
}

WebSocketChannelChild::~WebSocketChannelChild() {
  LOG(("WebSocketChannelChild::~WebSocketChannelChild() %p\n", this));
  mEventQ->NotifyReleasingOwner();
}

void WebSocketChannelChild::AddIPDLReference() {
  MOZ_ASSERT(NS_IsMainThread());

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mIPCState == Closed,
               "Attempt to retain more than one IPDL reference");
    mIPCState = Opened;
  }

  AddRef();
}

void WebSocketChannelChild::ReleaseIPDLReference() {
  MOZ_ASSERT(NS_IsMainThread());

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mIPCState != Closed,
               "Attempt to release nonexistent IPDL reference");
    mIPCState = Closed;
  }

  Release();
}

void WebSocketChannelChild::MaybeReleaseIPCObject() {
  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return;
    }

    mIPCState = Closing;
  }

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    MOZ_ALWAYS_SUCCEEDS(target->Dispatch(
        NewRunnableMethod("WebSocketChannelChild::MaybeReleaseIPCObject", this,
                          &WebSocketChannelChild::MaybeReleaseIPCObject),
        NS_DISPATCH_NORMAL));
    return;
  }

  SendDeleteSelf();
}

void WebSocketChannelChild::GetEffectiveURL(nsAString& aEffectiveURL) const {
  aEffectiveURL = mEffectiveURL;
}

bool WebSocketChannelChild::IsEncrypted() const { return mEncrypted; }

class WebSocketEvent {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(WebSocketEvent)
  MOZ_COUNTED_DTOR_VIRTUAL(WebSocketEvent)
  virtual void Run(WebSocketChannelChild* aChild) = 0;
};

class WrappedWebSocketEvent : public Runnable {
 public:
  WrappedWebSocketEvent(WebSocketChannelChild* aChild,
                        UniquePtr<WebSocketEvent>&& aWebSocketEvent)
      : Runnable("net::WrappedWebSocketEvent"),
        mChild(aChild),
        mWebSocketEvent(std::move(aWebSocketEvent)) {
    MOZ_RELEASE_ASSERT(!!mWebSocketEvent);
  }
  NS_IMETHOD Run() override {
    mWebSocketEvent->Run(mChild);
    return NS_OK;
  }

 private:
  RefPtr<WebSocketChannelChild> mChild;
  UniquePtr<WebSocketEvent> mWebSocketEvent;
};

class EventTargetDispatcher : public ChannelEvent {
 public:
  EventTargetDispatcher(WebSocketChannelChild* aChild,
                        WebSocketEvent* aWebSocketEvent)
      : mChild(aChild),
        mWebSocketEvent(aWebSocketEvent),
        mEventTarget(mChild->GetTargetThread()) {}

  void Run() override {
    if (mEventTarget) {
      mEventTarget->Dispatch(
          new WrappedWebSocketEvent(mChild, std::move(mWebSocketEvent)),
          NS_DISPATCH_NORMAL);
      return;
    }
  }

  already_AddRefed<nsIEventTarget> GetEventTarget() override {
    nsCOMPtr<nsIEventTarget> target = mEventTarget;
    if (!target) {
      target = GetMainThreadEventTarget();
    }
    return target.forget();
  }

 private:
  // The lifetime of the child is ensured by ChannelEventQueue.
  WebSocketChannelChild* mChild;
  UniquePtr<WebSocketEvent> mWebSocketEvent;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

class StartEvent : public WebSocketEvent {
 public:
  StartEvent(const nsACString& aProtocol, const nsACString& aExtensions,
             const nsAString& aEffectiveURL, bool aEncrypted,
             uint64_t aHttpChannelId)
      : mProtocol(aProtocol),
        mExtensions(aExtensions),
        mEffectiveURL(aEffectiveURL),
        mEncrypted(aEncrypted),
        mHttpChannelId(aHttpChannelId) {}

  void Run(WebSocketChannelChild* aChild) override {
    aChild->OnStart(mProtocol, mExtensions, mEffectiveURL, mEncrypted,
                    mHttpChannelId);
  }

 private:
  nsCString mProtocol;
  nsCString mExtensions;
  nsString mEffectiveURL;
  bool mEncrypted;
  uint64_t mHttpChannelId;
};

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnStart(
    const nsACString& aProtocol, const nsACString& aExtensions,
    const nsAString& aEffectiveURL, const bool& aEncrypted,
    const uint64_t& aHttpChannelId) {
  mEventQ->RunOrEnqueue(new EventTargetDispatcher(
      this, new StartEvent(aProtocol, aExtensions, aEffectiveURL, aEncrypted,
                           aHttpChannelId)));

  return IPC_OK();
}

void WebSocketChannelChild::OnStart(const nsACString& aProtocol,
                                    const nsACString& aExtensions,
                                    const nsAString& aEffectiveURL,
                                    const bool& aEncrypted,
                                    const uint64_t& aHttpChannelId) {
  LOG(("WebSocketChannelChild::RecvOnStart() %p\n", this));
  SetProtocol(aProtocol);
  mNegotiatedExtensions = aExtensions;
  mEffectiveURL = aEffectiveURL;
  mEncrypted = aEncrypted;
  mHttpChannelId = aHttpChannelId;

  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    nsresult rv = mListenerMT->mListener->OnStart(mListenerMT->mContext);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannelChild::OnStart "
           "mListenerMT->mListener->OnStart() failed with error 0x%08" PRIx32,
           static_cast<uint32_t>(rv)));
    }
  }
}

class StopEvent : public WebSocketEvent {
 public:
  explicit StopEvent(const nsresult& aStatusCode) : mStatusCode(aStatusCode) {}

  void Run(WebSocketChannelChild* aChild) override {
    aChild->OnStop(mStatusCode);
  }

 private:
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnStop(
    const nsresult& aStatusCode) {
  mEventQ->RunOrEnqueue(
      new EventTargetDispatcher(this, new StopEvent(aStatusCode)));

  return IPC_OK();
}

void WebSocketChannelChild::OnStop(const nsresult& aStatusCode) {
  LOG(("WebSocketChannelChild::RecvOnStop() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    nsresult rv =
        mListenerMT->mListener->OnStop(mListenerMT->mContext, aStatusCode);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannel::OnStop "
           "mListenerMT->mListener->OnStop() failed with error 0x%08" PRIx32,
           static_cast<uint32_t>(rv)));
    }
  }
}

class MessageEvent : public WebSocketEvent {
 public:
  MessageEvent(const nsACString& aMessage, bool aBinary)
      : mMessage(aMessage), mBinary(aBinary) {}

  void Run(WebSocketChannelChild* aChild) override {
    if (!mBinary) {
      aChild->OnMessageAvailable(mMessage);
    } else {
      aChild->OnBinaryMessageAvailable(mMessage);
    }
  }

 private:
  nsCString mMessage;
  bool mBinary;
};

bool WebSocketChannelChild::RecvOnMessageAvailableInternal(
    const nsACString& aMsg, bool aMoreData, bool aBinary) {
  if (aMoreData) {
    return mReceivedMsgBuffer.Append(aMsg, fallible);
  }

  if (!mReceivedMsgBuffer.Append(aMsg, fallible)) {
    return false;
  }

  mEventQ->RunOrEnqueue(new EventTargetDispatcher(
      this, new MessageEvent(mReceivedMsgBuffer, aBinary)));
  mReceivedMsgBuffer.Truncate();
  return true;
}

class OnErrorEvent : public WebSocketEvent {
 public:
  OnErrorEvent() = default;

  void Run(WebSocketChannelChild* aChild) override { aChild->OnError(); }
};

void WebSocketChannelChild::OnError() {
  LOG(("WebSocketChannelChild::OnError() %p", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    Unused << mListenerMT->mListener->OnError();
  }
}

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnMessageAvailable(
    const nsACString& aMsg, const bool& aMoreData) {
  if (!RecvOnMessageAvailableInternal(aMsg, aMoreData, false)) {
    LOG(("WebSocketChannelChild %p append message failed", this));
    mEventQ->RunOrEnqueue(new EventTargetDispatcher(this, new OnErrorEvent()));
  }
  return IPC_OK();
}

void WebSocketChannelChild::OnMessageAvailable(const nsACString& aMsg) {
  LOG(("WebSocketChannelChild::RecvOnMessageAvailable() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    nsresult rv =
        mListenerMT->mListener->OnMessageAvailable(mListenerMT->mContext, aMsg);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannelChild::OnMessageAvailable "
           "mListenerMT->mListener->OnMessageAvailable() "
           "failed with error 0x%08" PRIx32,
           static_cast<uint32_t>(rv)));
    }
  }
}

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnBinaryMessageAvailable(
    const nsACString& aMsg, const bool& aMoreData) {
  if (!RecvOnMessageAvailableInternal(aMsg, aMoreData, true)) {
    LOG(("WebSocketChannelChild %p append message failed", this));
    mEventQ->RunOrEnqueue(new EventTargetDispatcher(this, new OnErrorEvent()));
  }
  return IPC_OK();
}

void WebSocketChannelChild::OnBinaryMessageAvailable(const nsACString& aMsg) {
  LOG(("WebSocketChannelChild::RecvOnBinaryMessageAvailable() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    nsresult rv = mListenerMT->mListener->OnBinaryMessageAvailable(
        mListenerMT->mContext, aMsg);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannelChild::OnBinaryMessageAvailable "
           "mListenerMT->mListener->OnBinaryMessageAvailable() "
           "failed with error 0x%08" PRIx32,
           static_cast<uint32_t>(rv)));
    }
  }
}

class AcknowledgeEvent : public WebSocketEvent {
 public:
  explicit AcknowledgeEvent(const uint32_t& aSize) : mSize(aSize) {}

  void Run(WebSocketChannelChild* aChild) override {
    aChild->OnAcknowledge(mSize);
  }

 private:
  uint32_t mSize;
};

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnAcknowledge(
    const uint32_t& aSize) {
  mEventQ->RunOrEnqueue(
      new EventTargetDispatcher(this, new AcknowledgeEvent(aSize)));

  return IPC_OK();
}

void WebSocketChannelChild::OnAcknowledge(const uint32_t& aSize) {
  LOG(("WebSocketChannelChild::RecvOnAcknowledge() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    nsresult rv =
        mListenerMT->mListener->OnAcknowledge(mListenerMT->mContext, aSize);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannel::OnAcknowledge "
           "mListenerMT->mListener->OnAcknowledge() "
           "failed with error 0x%08" PRIx32,
           static_cast<uint32_t>(rv)));
    }
  }
}

class ServerCloseEvent : public WebSocketEvent {
 public:
  ServerCloseEvent(const uint16_t aCode, const nsACString& aReason)
      : mCode(aCode), mReason(aReason) {}

  void Run(WebSocketChannelChild* aChild) override {
    aChild->OnServerClose(mCode, mReason);
  }

 private:
  uint16_t mCode;
  nsCString mReason;
};

mozilla::ipc::IPCResult WebSocketChannelChild::RecvOnServerClose(
    const uint16_t& aCode, const nsACString& aReason) {
  mEventQ->RunOrEnqueue(
      new EventTargetDispatcher(this, new ServerCloseEvent(aCode, aReason)));

  return IPC_OK();
}

void WebSocketChannelChild::OnServerClose(const uint16_t& aCode,
                                          const nsACString& aReason) {
  LOG(("WebSocketChannelChild::RecvOnServerClose() %p\n", this));
  if (mListenerMT) {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    DebugOnly<nsresult> rv = mListenerMT->mListener->OnServerClose(
        mListenerMT->mContext, aCode, aReason);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void WebSocketChannelChild::SetupNeckoTarget() {
  mNeckoTarget = nsContentUtils::GetEventTargetByLoadInfo(
      mLoadInfo, TaskCategory::Network);
}

NS_IMETHODIMP
WebSocketChannelChild::AsyncOpen(nsIURI* aURI, const nsACString& aOrigin,
                                 JS::Handle<JS::Value> aOriginAttributes,
                                 uint64_t aInnerWindowID,
                                 nsIWebSocketListener* aListener,
                                 nsISupports* aContext, JSContext* aCx) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return AsyncOpenNative(aURI, aOrigin, attrs, aInnerWindowID, aListener,
                         aContext);
}

NS_IMETHODIMP
WebSocketChannelChild::AsyncOpenNative(
    nsIURI* aURI, const nsACString& aOrigin,
    const OriginAttributes& aOriginAttributes, uint64_t aInnerWindowID,
    nsIWebSocketListener* aListener, nsISupports* aContext) {
  LOG(("WebSocketChannelChild::AsyncOpen() %p\n", this));

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  MOZ_ASSERT((aURI && !mIsServerSide) || (!aURI && mIsServerSide),
             "Invalid aURI for WebSocketChannelChild::AsyncOpen");
  MOZ_ASSERT(aListener && !mListenerMT,
             "Invalid state for WebSocketChannelChild::AsyncOpen");

  mozilla::dom::BrowserChild* browserChild = nullptr;
  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsIBrowserChild),
                                getter_AddRefs(iBrowserChild));
  if (iBrowserChild) {
    browserChild =
        static_cast<mozilla::dom::BrowserChild*>(iBrowserChild.get());
  }

  ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  // Corresponding release in DeallocPWebSocket
  AddIPDLReference();

  nsCOMPtr<nsIURI> uri;
  Maybe<LoadInfoArgs> loadInfoArgs;
  Maybe<PTransportProviderChild*> transportProvider;

  if (!mIsServerSide) {
    uri = aURI;
    nsresult rv = LoadInfoToLoadInfoArgs(mLoadInfo, &loadInfoArgs);
    NS_ENSURE_SUCCESS(rv, rv);

    transportProvider = Nothing();
  } else {
    MOZ_ASSERT(mServerTransportProvider);
    PTransportProviderChild* ipcChild;
    nsresult rv = mServerTransportProvider->GetIPCChild(&ipcChild);
    NS_ENSURE_SUCCESS(rv, rv);

    transportProvider = Some(ipcChild);
  }

  // This must be called before sending constructor message.
  SetupNeckoTarget();

  gNeckoChild->SendPWebSocketConstructor(
      this, browserChild, IPC::SerializedLoadContext(this), mSerial);
  if (!SendAsyncOpen(uri, aOrigin, aOriginAttributes, aInnerWindowID, mProtocol,
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

class CloseEvent : public Runnable {
 public:
  CloseEvent(WebSocketChannelChild* aChild, uint16_t aCode,
             const nsACString& aReason)
      : Runnable("net::CloseEvent"),
        mChild(aChild),
        mCode(aCode),
        mReason(aReason) {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mChild->Close(mCode, mReason);
    return NS_OK;
  }

 private:
  RefPtr<WebSocketChannelChild> mChild;
  uint16_t mCode;
  nsCString mReason;
};

NS_IMETHODIMP
WebSocketChannelChild::Close(uint16_t code, const nsACString& reason) {
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    return target->Dispatch(new CloseEvent(this, code, reason),
                            NS_DISPATCH_NORMAL);
  }
  LOG(("WebSocketChannelChild::Close() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendClose(code, reason)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class MsgEvent : public Runnable {
 public:
  MsgEvent(WebSocketChannelChild* aChild, const nsACString& aMsg,
           bool aBinaryMsg)
      : Runnable("net::MsgEvent"),
        mChild(aChild),
        mMsg(aMsg),
        mBinaryMsg(aBinaryMsg) {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override {
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
  nsCString mMsg;
  bool mBinaryMsg;
};

NS_IMETHODIMP
WebSocketChannelChild::SendMsg(const nsACString& aMsg) {
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    return target->Dispatch(new MsgEvent(this, aMsg, false),
                            NS_DISPATCH_NORMAL);
  }
  LOG(("WebSocketChannelChild::SendMsg() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendMsg(aMsg)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelChild::SendBinaryMsg(const nsACString& aMsg) {
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    return target->Dispatch(new MsgEvent(this, aMsg, true), NS_DISPATCH_NORMAL);
  }
  LOG(("WebSocketChannelChild::SendBinaryMsg() %p\n", this));

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendBinaryMsg(aMsg)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class BinaryStreamEvent : public Runnable {
 public:
  BinaryStreamEvent(WebSocketChannelChild* aChild, nsIInputStream* aStream,
                    uint32_t aLength)
      : Runnable("net::BinaryStreamEvent"),
        mChild(aChild),
        mStream(aStream),
        mLength(aLength) {
    MOZ_RELEASE_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsresult rv = mChild->SendBinaryStream(mStream, mLength);
    if (NS_FAILED(rv)) {
      LOG(
          ("WebSocketChannelChild::BinaryStreamEvent %p "
           "SendBinaryStream failed (%08" PRIx32 ")\n",
           this, static_cast<uint32_t>(rv)));
    }
    return NS_OK;
  }

 private:
  RefPtr<WebSocketChannelChild> mChild;
  nsCOMPtr<nsIInputStream> mStream;
  uint32_t mLength;
};

NS_IMETHODIMP
WebSocketChannelChild::SendBinaryStream(nsIInputStream* aStream,
                                        uint32_t aLength) {
  if (!NS_IsMainThread()) {
    MOZ_RELEASE_ASSERT(IsOnTargetThread());
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    return target->Dispatch(new BinaryStreamEvent(this, aStream, aLength),
                            NS_DISPATCH_NORMAL);
  }

  LOG(("WebSocketChannelChild::SendBinaryStream() %p\n", this));

  IPCStream ipcStream;
  if (NS_WARN_IF(!mozilla::ipc::SerializeIPCStream(do_AddRef(aStream),
                                                   ipcStream,
                                                   /* aAllowLazy */ false))) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    MutexAutoLock lock(mMutex);
    if (mIPCState != Opened) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (!SendSendBinaryStream(ipcStream, aLength)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelChild::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  LOG(("WebSocketChannelChild::GetSecurityInfo() %p\n", this));
  return NS_ERROR_NOT_AVAILABLE;
}

}  // namespace net
}  // namespace mozilla
