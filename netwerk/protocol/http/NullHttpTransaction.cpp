/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "NullHttpTransaction.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsIHttpActivityObserver.h"
#include "NullHttpChannel.h"
#include "nsQueryObject.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

class CallObserveActivity final : public nsIRunnable
{
  ~CallObserveActivity()
  {
  }
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  CallObserveActivity(nsIHttpActivityObserver *aActivityDistributor,
                      const nsCString &aHost,
                      int32_t aPort,
                      bool aEndToEndSSL,
                      uint32_t aActivityType,
                      uint32_t aActivitySubtype,
                      PRTime aTimestamp,
                      uint64_t aExtraSizeData,
                      const nsACString &aExtraStringData)
    : mActivityDistributor(aActivityDistributor)
    , mHost(aHost)
    , mPort(aPort)
    , mEndToEndSSL(aEndToEndSSL)
    , mActivityType(aActivityType)
    , mActivitySubtype(aActivitySubtype)
    , mTimestamp(aTimestamp)
    , mExtraSizeData(aExtraSizeData)
    , mExtraStringData(aExtraStringData)
  {
  }
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIURI> uri;
    nsAutoCString port(NS_LITERAL_CSTRING(""));
    if (mPort != -1 && ((mEndToEndSSL && mPort != 443) ||
        (!mEndToEndSSL && mPort != 80))) {
      port.AppendInt(mPort);
    }

    nsresult rv = NS_NewURI(getter_AddRefs(uri),
                            (mEndToEndSSL ? NS_LITERAL_CSTRING("https://")
                            : NS_LITERAL_CSTRING("http://") ) + mHost + port);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    RefPtr<NullHttpChannel> channel = new NullHttpChannel();
    channel->Init(uri, 0, nullptr, 0, nullptr);
    mActivityDistributor->ObserveActivity(
      nsCOMPtr<nsISupports>(do_QueryObject(channel)),
      mActivityType,
      mActivitySubtype,
      mTimestamp,
      mExtraSizeData,
      mExtraStringData);

    return NS_OK;
  }
private:
  nsCOMPtr<nsIHttpActivityObserver> mActivityDistributor;
  nsCString mHost;
  int32_t mPort;
  bool mEndToEndSSL;
  uint32_t mActivityType;
  uint32_t mActivitySubtype;
  PRTime mTimestamp;
  uint64_t mExtraSizeData;
  nsCString mExtraStringData;
};

NS_IMPL_ISUPPORTS(CallObserveActivity, nsIRunnable)

NS_IMPL_ISUPPORTS(NullHttpTransaction, NullHttpTransaction, nsISupportsWeakReference)

NullHttpTransaction::NullHttpTransaction(nsHttpConnectionInfo *ci,
                                         nsIInterfaceRequestor *callbacks,
                                         uint32_t caps)
  : mStatus(NS_OK)
  , mCaps(caps | NS_HTTP_ALLOW_KEEPALIVE)
  , mRequestHead(nullptr)
  , mCapsToClear(0)
  , mIsDone(false)
  , mClaimed(false)
  , mCallbacks(callbacks)
  , mConnectionInfo(ci)
{
  nsresult rv;
  mActivityDistributor = do_GetService(NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID,
                                       &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  bool activityDistributorActive;
  rv = mActivityDistributor->GetIsActive(&activityDistributorActive);
  if (NS_SUCCEEDED(rv) && activityDistributorActive) {
    // There are some observers registered at activity distributor.
    LOG(("NulHttpTransaction::NullHttpTransaction() "
         "mActivityDistributor is active "
         "[this=%p, %s]", this, ci->GetOrigin().get()));
  } else {
    // There is no observer, so don't use it.
    mActivityDistributor = nullptr;
  }
}

NullHttpTransaction::~NullHttpTransaction()
{
  mCallbacks = nullptr;
  delete mRequestHead;
}

bool
NullHttpTransaction::Claim()
{
  if (mClaimed) {
    return false;
  }
  mClaimed = true;
  return true;
}

void
NullHttpTransaction::SetConnection(nsAHttpConnection *conn)
{
  mConnection = conn;
}

nsAHttpConnection *
NullHttpTransaction::Connection()
{
  return mConnection.get();
}

void
NullHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **outCB)
{
  nsCOMPtr<nsIInterfaceRequestor> copyCB(mCallbacks);
  *outCB = copyCB.forget().take();
}

void
NullHttpTransaction::OnTransportStatus(nsITransport* transport,
                                       nsresult status, int64_t progress)
{
  if (mActivityDistributor) {
    NS_DispatchToMainThread(new CallObserveActivity(mActivityDistributor,
                                  mConnectionInfo->GetOrigin(),
                                  mConnectionInfo->OriginPort(),
                                  mConnectionInfo->EndToEndSSL(),
                                  NS_HTTP_ACTIVITY_TYPE_SOCKET_TRANSPORT,
                                  static_cast<uint32_t>(status),
                                  PR_Now(),
                                  progress,
                                  EmptyCString()));
  }
}

bool
NullHttpTransaction::IsDone()
{
  return mIsDone;
}

nsresult
NullHttpTransaction::Status()
{
  return mStatus;
}

uint32_t
NullHttpTransaction::Caps()
{
  return mCaps & ~mCapsToClear;
}

void
NullHttpTransaction::SetDNSWasRefreshed()
{
  MOZ_ASSERT(NS_IsMainThread(), "SetDNSWasRefreshed on main thread only!");
  mCapsToClear |= NS_HTTP_REFRESH_DNS;
}

uint64_t
NullHttpTransaction::Available()
{
  return 0;
}

nsresult
NullHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                  uint32_t count, uint32_t *countRead)
{
  *countRead = 0;
  mIsDone = true;
  return NS_BASE_STREAM_CLOSED;
}

nsresult
NullHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                   uint32_t count, uint32_t *countWritten)
{
  *countWritten = 0;
  return NS_BASE_STREAM_CLOSED;
}

uint32_t
NullHttpTransaction::Http1xTransactionCount()
{
  return 0;
}

nsHttpRequestHead *
NullHttpTransaction::RequestHead()
{
  // We suport a requesthead at all so that a CONNECT tunnel transaction
  // can obtain a Host header from it, but we lazy-popualate that header.

  if (!mRequestHead) {
    mRequestHead = new nsHttpRequestHead();

    nsAutoCString hostHeader;
    nsCString host(mConnectionInfo->GetOrigin());
    nsresult rv = nsHttpHandler::GenerateHostPort(host,
                                                  mConnectionInfo->OriginPort(),
                                                  hostHeader);
    if (NS_SUCCEEDED(rv)) {
      mRequestHead->SetHeader(nsHttp::Host, hostHeader);
      if (mActivityDistributor) {
        // Report request headers.
        nsCString reqHeaderBuf;
        mRequestHead->Flatten(reqHeaderBuf, false);
        NS_DispatchToMainThread(new CallObserveActivity(mActivityDistributor,
                                  mConnectionInfo->GetOrigin(),
                                  mConnectionInfo->OriginPort(),
                                  mConnectionInfo->EndToEndSSL(),
                                  NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                                  NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_HEADER,
                                  PR_Now(), 0, reqHeaderBuf));
    }
  }

    // CONNECT tunnels may also want Proxy-Authorization but that is a lot
    // harder to determine, so for now we will let those connections fail in
    // the NullHttpTransaction and let them be retried from the pending queue
    // with a bound transaction
  }

  return mRequestHead;
}

nsresult
NullHttpTransaction::TakeSubTransactions(
  nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
NullHttpTransaction::SetProxyConnectFailed()
{
}

void
NullHttpTransaction::Close(nsresult reason)
{
  mStatus = reason;
  mConnection = nullptr;
  mIsDone = true;
  if (mActivityDistributor) {
    // Report that this transaction is closing.
    NS_DispatchToMainThread(new CallObserveActivity(mActivityDistributor,
                                  mConnectionInfo->GetOrigin(),
                                  mConnectionInfo->OriginPort(),
                                  mConnectionInfo->EndToEndSSL(),
                                  NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                                  NS_HTTP_ACTIVITY_SUBTYPE_TRANSACTION_CLOSE,
                                  PR_Now(), 0, EmptyCString()));
  }
}

nsHttpConnectionInfo *
NullHttpTransaction::ConnectionInfo()
{
  return mConnectionInfo;
}

nsresult
NullHttpTransaction::AddTransaction(nsAHttpTransaction *trans)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
NullHttpTransaction::PipelineDepth()
{
  return 0;
}

nsresult
NullHttpTransaction::SetPipelinePosition(int32_t position)
{
    return NS_OK;
}

int32_t
NullHttpTransaction::PipelinePosition()
{
  return 1;
}

} // namespace net
} // namespace mozilla

