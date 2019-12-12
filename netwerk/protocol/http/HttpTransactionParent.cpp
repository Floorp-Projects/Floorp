/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpTransactionParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsHttpHandler.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(HttpTransactionParent)
NS_IMPL_QUERY_INTERFACE(HttpTransactionParent, nsIRequest,
                        nsIThreadRetargetableRequest)

NS_IMETHODIMP_(MozExternalRefCountType) HttpTransactionParent::Release(void) {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");

  if (!mRefCnt.isThreadSafe) {
    NS_ASSERT_OWNINGTHREAD(HttpTransactionParent);
  }

  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "HttpTransactionParent");

  if (count == 0) {
    if (!nsAutoRefCnt::isThreadSafe) {
      NS_ASSERT_OWNINGTHREAD(HttpTransactionParent);
    }

    mRefCnt = 1; /* stabilize */
    delete (this);
    return 0;
  }

  // When ref count goes down to 1 (held internally by IPDL), it means that
  // we are done with this transaction. We should send a delete message
  // to delete the transaction child in socket process.
  if (count == 1 && CanSend()) {
    mozilla::Unused << Send__delete__(this);
    return 1;
  }
  return count;
}

//-----------------------------------------------------------------------------
// HttpTransactionParent <public>
//-----------------------------------------------------------------------------

HttpTransactionParent::HttpTransactionParent()
    : mResponseIsComplete(false),
      mTransferSize(0),
      mRequestSize(0),
      mProxyConnectFailed(false),
      mCanceled(false),
      mStatus(NS_OK),
      mSuspendCount(0),
      mResponseHeadTaken(false),
      mResponseTrailersTaken(false),
      mHasStickyConnection(false),
      mOnStartRequestCalled(false),
      mOnStopRequestCalled(false) {
  LOG(("Creating HttpTransactionParent @%p\n", this));

  this->mSelfAddr.inet = {};
  this->mPeerAddr.inet = {};

#ifdef MOZ_VALGRIND
  memset(&mSelfAddr, 0, sizeof(NetAddr));
  memset(&mPeerAddr, 0, sizeof(NetAddr));
#endif
  mSelfAddr.raw.family = PR_AF_UNSPEC;
  mPeerAddr.raw.family = PR_AF_UNSPEC;

  mEventQ = new ChannelEventQueue(static_cast<nsIRequest*>(this));
}

HttpTransactionParent::~HttpTransactionParent() {
  LOG(("Destroying HttpTransactionParent @%p\n", this));
}

void HttpTransactionParent::GetStructFromInfo(
    nsHttpConnectionInfo* aInfo, HttpConnectionInfoCloneArgs& aArgs) {
  aArgs.host() = aInfo->GetOrigin();
  aArgs.port() = aInfo->OriginPort();
  aArgs.npnToken() = aInfo->GetNPNToken();
  aArgs.username() = aInfo->GetUsername();
  aArgs.originAttributes() = aInfo->GetOriginAttributes();
  aArgs.endToEndSSL() = aInfo->EndToEndSSL();
  aArgs.routedHost() = aInfo->GetRoutedHost();
  aArgs.routedPort() = aInfo->RoutedPort();
  aArgs.anonymous() = aInfo->GetAnonymous();
  aArgs.aPrivate() = aInfo->GetPrivate();
  aArgs.insecureScheme() = aInfo->GetInsecureScheme();
  aArgs.noSpdy() = aInfo->GetNoSpdy();
  aArgs.beConservative() = aInfo->GetBeConservative();
  aArgs.tlsFlags() = aInfo->GetTlsFlags();
  aArgs.isolated() = aInfo->GetIsolated();
  aArgs.isTrrServiceChannel() = aInfo->GetIsTrrServiceChannel();
  aArgs.trrDisabled() = aInfo->GetTrrDisabled();
  aArgs.isIPv4Disabled() = aInfo->GetIPv4Disabled();
  aArgs.isIPv6Disabled() = aInfo->GetIPv6Disabled();
  aArgs.topWindowOrigin() = aInfo->GetTopWindowOrigin();
  aArgs.isHttp3() = aInfo->IsHttp3();

  if (!aInfo->ProxyInfo()) {
    return;
  }

  nsTArray<ProxyInfoCloneArgs> proxyInfoArray;
  nsProxyInfo* head = aInfo->ProxyInfo();
  for (nsProxyInfo* iter = head; iter; iter = iter->mNext) {
    ProxyInfoCloneArgs* arg = proxyInfoArray.AppendElement();
    arg->type() = nsCString(iter->Type());
    arg->host() = aInfo->ProxyInfo()->Host();
    arg->port() = aInfo->ProxyInfo()->Port();
    arg->username() = aInfo->ProxyInfo()->Username();
    arg->password() = aInfo->ProxyInfo()->Password();
    arg->proxyAuthorizationHeader() =
        aInfo->ProxyInfo()->ProxyAuthorizationHeader();
    arg->connectionIsolationKey() =
        aInfo->ProxyInfo()->ConnectionIsolationKey();
    arg->flags() = aInfo->ProxyInfo()->Flags();
    arg->timeout() = aInfo->ProxyInfo()->Timeout();
    arg->resolveFlags() = aInfo->ProxyInfo()->ResolveFlags();
  }
  aArgs.proxyInfo() = proxyInfoArray;
}

//-----------------------------------------------------------------------------
// HttpTransactionParent <nsAHttpTransactionShell>
//-----------------------------------------------------------------------------

// Let socket process init the *real* nsHttpTransaction.
nsresult HttpTransactionParent::Init(
    uint32_t caps, nsHttpConnectionInfo* cinfo, nsHttpRequestHead* requestHead,
    nsIInputStream* requestBody, uint64_t requestContentLength,
    bool requestBodyHasHeaders, nsIEventTarget* target,
    nsIInterfaceRequestor* callbacks, nsITransportEventSink* eventsink,
    uint64_t topLevelOuterContentWindowId,
    HttpTrafficCategory trafficCategory) {
  LOG(("HttpTransactionParent::Init [this=%p caps=%x]\n", this, caps));

  if (!CanSend()) {
    return NS_ERROR_FAILURE;
  }

  mEventsink = eventsink;
  mTargetThread = GetCurrentThreadEventTarget();

  HttpConnectionInfoCloneArgs infoArgs;
  GetStructFromInfo(cinfo, infoArgs);

  mozilla::ipc::AutoIPCStream autoStream;
  if (requestBody &&
      !autoStream.Serialize(requestBody, SocketProcessParent::GetSingleton())) {
    return NS_ERROR_FAILURE;
  }

  // TODO: Figure out if we have to implement nsIThreadRetargetableRequest in
  // bug 1544378.
  if (!SendInit(caps, infoArgs, *requestHead,
                requestBody ? Some(autoStream.TakeValue()) : Nothing(),
                requestContentLength, requestBodyHasHeaders,
                topLevelOuterContentWindowId,
                static_cast<uint8_t>(trafficCategory))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult HttpTransactionParent::AsyncRead(nsIStreamListener* listener,
                                          nsIRequest** pump) {
  MOZ_ASSERT(pump);

  NS_ADDREF(*pump = this);
  mChannel = listener;
  return NS_OK;
}

void HttpTransactionParent::SetClassOfService(uint32_t classOfService) {
  Unused << SendUpdateClassOfService(classOfService);
}

nsHttpResponseHead* HttpTransactionParent::TakeResponseHead() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResponseHeadTaken, "TakeResponseHead called 2x");

  mResponseHeadTaken = true;
  return mResponseHead.forget();
}

nsHttpHeaderArray* HttpTransactionParent::TakeResponseTrailers() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResponseTrailersTaken, "TakeResponseTrailers called 2x");

  mResponseTrailersTaken = true;
  return mResponseTrailers.forget();
}

nsresult HttpTransactionParent::SetSniffedTypeToChannel(
    nsIRequest* aPump, nsIChannel* aChannel,
    nsInputStreamPump::PeekSegmentFun aCallTypeSniffers) {
  // TODO: will be implemented later in bug 1600254.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpTransactionParent::GetDeliveryTarget(nsIEventTarget** aNewTarget) {
  nsCOMPtr<nsIEventTarget> target = mTargetThread;
  target.forget(aNewTarget);
  return NS_OK;
}

NS_IMETHODIMP HttpTransactionParent::RetargetDeliveryTo(
    nsIEventTarget* aEventTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void HttpTransactionParent::SetDNSWasRefreshed() {
  MOZ_ASSERT(NS_IsMainThread(), "SetDNSWasRefreshed on main thread only!");
  Unused << SendSetDNSWasRefreshed();
}

void HttpTransactionParent::GetNetworkAddresses(NetAddr& self, NetAddr& peer,
                                                bool& aResolvedByTRR) {
  self = mSelfAddr;
  peer = mPeerAddr;

  // TODO: will be implemented later in bug 1600254.
  aResolvedByTRR = false;
}

bool HttpTransactionParent::HasStickyConnection() const {
  return mHasStickyConnection;
}

mozilla::TimeStamp HttpTransactionParent::GetDomainLookupStart() {
  return mTimings.domainLookupStart;
}

mozilla::TimeStamp HttpTransactionParent::GetDomainLookupEnd() {
  return mTimings.domainLookupEnd;
}

mozilla::TimeStamp HttpTransactionParent::GetConnectStart() {
  return mTimings.connectStart;
}

mozilla::TimeStamp HttpTransactionParent::GetTcpConnectEnd() {
  return mTimings.tcpConnectEnd;
}

mozilla::TimeStamp HttpTransactionParent::GetSecureConnectionStart() {
  return mTimings.secureConnectionStart;
}

mozilla::TimeStamp HttpTransactionParent::GetConnectEnd() {
  return mTimings.connectEnd;
}

mozilla::TimeStamp HttpTransactionParent::GetRequestStart() {
  return mTimings.requestStart;
}

mozilla::TimeStamp HttpTransactionParent::GetResponseStart() {
  return mTimings.responseStart;
}

mozilla::TimeStamp HttpTransactionParent::GetResponseEnd() {
  return mTimings.responseEnd;
}

const TimingStruct HttpTransactionParent::Timings() { return mTimings; }

bool HttpTransactionParent::ResponseIsComplete() { return mResponseIsComplete; }

int64_t HttpTransactionParent::GetTransferSize() { return mTransferSize; }

int64_t HttpTransactionParent::GetRequestSize() { return mRequestSize; }

nsISupports* HttpTransactionParent::SecurityInfo() { return mSecurityInfo; }

bool HttpTransactionParent::ProxyConnectFailed() { return mProxyConnectFailed; }

void HttpTransactionParent::SetTransactionObserver(TransactionObserver* arg) {
  // TODO: will be implemented later in bug 1600254.
}

void HttpTransactionParent::DontReuseConnection() {
  MOZ_ASSERT(NS_IsMainThread());
  Unused << SendDontReuseConnection();
}

void HttpTransactionParent::SetH2WSConnRefTaken() {
  MOZ_ASSERT(NS_IsMainThread());
  Unused << SendSetH2WSConnRefTaken();
}

void HttpTransactionParent::SetSecurityCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  // TODO: we might don't need to implement this.
  // Will figure out in bug 1512479.
}

void HttpTransactionParent::SetRequestContext(
    nsIRequestContext* aRequestContext) {
  // TODO: will be implemented later in bug 1600254.
}
void HttpTransactionParent::SetPushedStream(Http2PushedStreamWrapper* push) {
  // TODO: will be implemented later in bug 1600254.
}

void HttpTransactionParent::SetDomainLookupStart(mozilla::TimeStamp timeStamp,
                                                 bool onlyIfNull) {
  // TODO: will be implemented later in bug 1600254.
}
void HttpTransactionParent::SetDomainLookupEnd(mozilla::TimeStamp timeStamp,
                                               bool onlyIfNull) {
  // TODO: will be implemented later in bug 1600254.
}

nsHttpTransaction* HttpTransactionParent::AsHttpTransaction() {
  return nullptr;
}

HttpTransactionParent* HttpTransactionParent::AsHttpTransactionParent() {
  return this;
}

int32_t HttpTransactionParent::GetProxyConnectResponseCode() {
  // TODO: will be implemented later in bug 1600254.
  return 0;
}

already_AddRefed<nsIEventTarget> HttpTransactionParent::GetNeckoTarget() {
  nsCOMPtr<nsIEventTarget> target = GetMainThreadEventTarget();
  return target.forget();
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnStartRequest(
    const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
    const nsCString& aSecurityInfoSerialization,
    const bool& aProxyConnectFailed, const TimingStructArgs& aTimings) {
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this,
      [self = UnsafePtr<HttpTransactionParent>(this), aStatus, aResponseHead,
       aSecurityInfoSerialization, aProxyConnectFailed, aTimings]() {
        self->DoOnStartRequest(aStatus, aResponseHead,
                               aSecurityInfoSerialization, aProxyConnectFailed,
                               aTimings);
      }));
  return IPC_OK();
}

static void TimingStructArgsToTimingsStruct(const TimingStructArgs& aArgs,
                                            TimingStruct& aTimings) {
  aTimings.domainLookupStart = aArgs.domainLookupStart();
  aTimings.domainLookupEnd = aArgs.domainLookupEnd();
  aTimings.connectStart = aArgs.connectStart();
  aTimings.tcpConnectEnd = aArgs.tcpConnectEnd();
  aTimings.secureConnectionStart = aArgs.secureConnectionStart();
  aTimings.connectEnd = aArgs.connectEnd();
  aTimings.requestStart = aArgs.requestStart();
  aTimings.responseStart = aArgs.responseStart();
  aTimings.responseEnd = aArgs.responseEnd();
}

void HttpTransactionParent::DoOnStartRequest(
    const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
    const nsCString& aSecurityInfoSerialization,
    const bool& aProxyConnectFailed, const TimingStructArgs& aTimings) {
  LOG(("HttpTransactionParent::DoOnStartRequest [this=%p aStatus=%" PRIx32
       "]\n",
       this, static_cast<uint32_t>(aStatus)));

  if (mCanceled) {
    return;
  }

  MOZ_ASSERT(!mOnStartRequestCalled);

  mStatus = aStatus;

  if (!aSecurityInfoSerialization.IsEmpty()) {
    NS_DeserializeObject(aSecurityInfoSerialization,
                         getter_AddRefs(mSecurityInfo));
  }

  if (aResponseHead.isSome()) {
    mResponseHead = new nsHttpResponseHead(aResponseHead.ref());
  }
  mProxyConnectFailed = aProxyConnectFailed;
  TimingStructArgsToTimingsStruct(aTimings, mTimings);

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  nsresult rv = mChannel->OnStartRequest(this);
  mOnStartRequestCalled = true;
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnTransportStatus(
    const nsresult& aStatus, const int64_t& aProgress,
    const int64_t& aProgressMax) {
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<HttpTransactionParent>(this), aStatus, aProgress,
             aProgressMax]() {
        self->DoOnTransportStatus(aStatus, aProgress, aProgressMax);
      }));
  return IPC_OK();
}

void HttpTransactionParent::DoOnTransportStatus(const nsresult& aStatus,
                                                const int64_t& aProgress,
                                                const int64_t& aProgressMax) {
  LOG(("HttpTransactionParent::DoOnTransportStatus [this=%p]\n", this));
  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  mEventsink->OnTransportStatus(nullptr, aStatus, aProgress, aProgressMax);
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnDataAvailable(
    const nsCString& aData, const uint64_t& aOffset, const uint32_t& aCount) {
  LOG(("HttpTransactionParent::RecvOnDataAvailable [this=%p, aOffset= %" PRIu64
       " aCount=%" PRIu32,
       this, aOffset, aCount));

  if (mCanceled) {
    return IPC_OK();
  }

  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<HttpTransactionParent>(this), aData, aOffset,
             aCount]() { self->DoOnDataAvailable(aData, aOffset, aCount); }));
  return IPC_OK();
}

void HttpTransactionParent::DoOnDataAvailable(const nsCString& aData,
                                              const uint64_t& aOffset,
                                              const uint32_t& aCount) {
  LOG(("HttpTransactionParent::DoOnDataAvailable [this=%p]\n", this));
  if (mCanceled) {
    return;
  }

  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream),
                                      MakeSpan(aData.get(), aCount),
                                      NS_ASSIGNMENT_DEPEND);

  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  rv = mChannel->OnDataAvailable(this, stringStream, aOffset, aCount);
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnStopRequest(
    const nsresult& aStatus, const bool& aResponseIsComplete,
    const int64_t& aTransferSize, const TimingStructArgs& aTimings,
    const Maybe<nsHttpHeaderArray>& aResponseTrailers,
    const bool& aHasStickyConn) {
  LOG(("HttpTransactionParent::RecvOnStopRequest [this=%p status=%" PRIx32
       "]\n",
       this, static_cast<uint32_t>(aStatus)));
  mEventQ->RunOrEnqueue(new NeckoTargetChannelFunctionEvent(
      this, [self = UnsafePtr<HttpTransactionParent>(this), aStatus,
             aResponseIsComplete, aTransferSize, aTimings, aResponseTrailers,
             aHasStickyConn]() {
        self->DoOnStopRequest(aStatus, aResponseIsComplete, aTransferSize,
                              aTimings, aResponseTrailers, aHasStickyConn);
      }));
  return IPC_OK();
}

void HttpTransactionParent::DoOnStopRequest(
    const nsresult& aStatus, const bool& aResponseIsComplete,
    const int64_t& aTransferSize, const TimingStructArgs& aTimings,
    const Maybe<nsHttpHeaderArray>& aResponseTrailers,
    const bool& aHasStickyConn) {
  LOG(("HttpTransactionParent::DoOnStopRequest [this=%p]\n", this));
  if (mCanceled) {
    return;
  }

  MOZ_ASSERT(!mOnStopRequestCalled, "We should not call OnStopRequest twice");

  mStatus = aStatus;

  nsCOMPtr<nsIRequest> deathGrip = this;

  mResponseIsComplete = aResponseIsComplete;
  mTransferSize = aTransferSize;
  TimingStructArgsToTimingsStruct(aTimings, mTimings);

  if (aResponseTrailers.isSome()) {
    mResponseTrailers = new nsHttpHeaderArray(aResponseTrailers.ref());
  }
  mHasStickyConnection = aHasStickyConn;

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  Unused << mChannel->OnStopRequest(this, mStatus);
  mOnStopRequestCalled = true;
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnNetAddrUpdate(
    const NetAddr& aSelfAddr, const NetAddr& aPeerAddr) {
  mSelfAddr = aSelfAddr;
  mPeerAddr = aPeerAddr;
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpTransactionParent::RecvOnInitFailed(
    const nsresult& aStatus) {
  nsCOMPtr<nsIRequest> request = do_QueryInterface(mEventsink);
  if (request) {
    request->Cancel(aStatus);
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpTransactionParent <nsIRequest>
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpTransactionParent::GetName(nsACString& aResult) {
  aResult.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionParent::IsPending(bool* aRetval) {
  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionParent::GetStatus(nsresult* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionParent::Cancel(nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("HttpTransactionParent::Cancel [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aStatus)));

  if (mCanceled) {
    LOG(("  already canceled\n"));
    return NS_OK;
  }

  MOZ_ASSERT(NS_FAILED(aStatus), "cancel with non-failure status code");

  mCanceled = true;
  mStatus = aStatus;
  if (CanSend()) {
    Unused << SendCancelPump(mStatus);
  }

  // Put DoNotifyListener() in front of the queue to avoid OnDataAvailable
  // being called after cancellation. Note that
  // HttpTransactionParent::OnStart/StopRequest are driven by IPC messages and
  // HttpTransactionChild won't send IPC if already canceled. That's why we have
  // to call DoNotifyListener().
  mEventQ->Suspend();
  mEventQ->PrependEvent(MakeUnique<NeckoTargetChannelFunctionEvent>(
      this, [self = UnsafePtr<HttpTransactionParent>(this)]() {
        self->DoNotifyListener();
      }));
  mEventQ->Resume();
  return NS_OK;
}

void HttpTransactionParent::DoNotifyListener() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOnStartRequestCalled) {
    mChannel->OnStartRequest(this);
    mOnStartRequestCalled = true;
  }

  if (!mOnStopRequestCalled) {
    mChannel->OnStopRequest(this, mStatus);
    mOnStopRequestCalled = true;
  }
}

NS_IMETHODIMP
HttpTransactionParent::Suspend() {
  MOZ_ASSERT(NS_IsMainThread());

  // SendSuspend only once, when suspend goes from 0 to 1.
  if (!mSuspendCount++ && CanSend()) {
    Unused << SendSuspendPump();
  }
  mEventQ->Suspend();
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionParent::Resume() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSuspendCount, "Resume called more than Suspend");

  // SendResume only once, when suspend count drops to 0.
  if (mSuspendCount && !--mSuspendCount && CanSend()) {
    Unused << SendResumePump();
  }
  mEventQ->Resume();
  return NS_OK;
}

NS_IMETHODIMP
HttpTransactionParent::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  MOZ_ASSERT(false, "Should not be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpTransactionParent::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  MOZ_ASSERT(false, "Should not be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpTransactionParent::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  MOZ_ASSERT(false, "Should not be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpTransactionParent::SetLoadFlags(nsLoadFlags aLoadFlags) {
  MOZ_ASSERT(false, "Should not be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void HttpTransactionParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("HttpTransactionParent::ActorDestroy [this=%p]\n", this));
  if (aWhy != Deletion) {
    Cancel(NS_ERROR_FAILURE);
  }
}

}  // namespace net
}  // namespace mozilla
