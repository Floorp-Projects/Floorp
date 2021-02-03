/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/net/NeckoChannelParams.h"  // For HttpActivityArgs.
#include "nsHttp.h"
#include "NullHttpTransaction.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsIHttpActivityObserver.h"
#include "nsQueryObject.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NullHttpTransaction, NullHttpTransaction,
                  nsISupportsWeakReference)

NullHttpTransaction::NullHttpTransaction(nsHttpConnectionInfo* ci,
                                         nsIInterfaceRequestor* callbacks,
                                         uint32_t caps)
    : mStatus(NS_OK),
      mCaps(caps | NS_HTTP_ALLOW_KEEPALIVE),
      mRequestHead(nullptr),
      mIsDone(false),
      mClaimed(false),
      mCallbacks(callbacks),
      mConnectionInfo(ci) {
  nsresult rv;
  mActivityDistributor =
      do_GetService(NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  bool activityDistributorActive;
  rv = mActivityDistributor->GetIsActive(&activityDistributorActive);
  if (NS_SUCCEEDED(rv) && activityDistributorActive) {
    // There are some observers registered at activity distributor.
    LOG(
        ("NulHttpTransaction::NullHttpTransaction() "
         "mActivityDistributor is active "
         "[this=%p, %s]",
         this, ci->GetOrigin().get()));
  } else {
    // There is no observer, so don't use it.
    mActivityDistributor = nullptr;
  }
}

NullHttpTransaction::~NullHttpTransaction() {
  mCallbacks = nullptr;
  delete mRequestHead;
}

bool NullHttpTransaction::Claim() {
  if (mClaimed) {
    return false;
  }
  mClaimed = true;
  return true;
}

void NullHttpTransaction::Unclaim() { mClaimed = false; }

void NullHttpTransaction::SetConnection(nsAHttpConnection* conn) {
  mConnection = conn;
}

nsAHttpConnection* NullHttpTransaction::Connection() {
  return mConnection.get();
}

void NullHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor** outCB) {
  nsCOMPtr<nsIInterfaceRequestor> copyCB(mCallbacks);
  *outCB = copyCB.forget().take();
}

void NullHttpTransaction::OnTransportStatus(nsITransport* transport,
                                            nsresult status, int64_t progress) {
  if (status == NS_NET_STATUS_RESOLVING_HOST) {
    if (mTimings.domainLookupStart.IsNull()) {
      mTimings.domainLookupStart = TimeStamp::Now();
    }
  } else if (status == NS_NET_STATUS_RESOLVED_HOST) {
    if (mTimings.domainLookupEnd.IsNull()) {
      mTimings.domainLookupEnd = TimeStamp::Now();
    }
  } else if (status == NS_NET_STATUS_CONNECTING_TO) {
    if (mTimings.connectStart.IsNull()) {
      mTimings.connectStart = TimeStamp::Now();
    }
  } else if (status == NS_NET_STATUS_CONNECTED_TO) {
    TimeStamp tnow = TimeStamp::Now();
    if (mTimings.connectEnd.IsNull()) {
      mTimings.connectEnd = tnow;
    }
    if (mTimings.tcpConnectEnd.IsNull()) {
      mTimings.tcpConnectEnd = tnow;
    }
  } else if (status == NS_NET_STATUS_TLS_HANDSHAKE_STARTING) {
    if (mTimings.secureConnectionStart.IsNull()) {
      mTimings.secureConnectionStart = TimeStamp::Now();
    }
  } else if (status == NS_NET_STATUS_TLS_HANDSHAKE_ENDED) {
    mTimings.connectEnd = TimeStamp::Now();
    ;
  }

  if (mActivityDistributor) {
    Unused << mActivityDistributor->ObserveActivityWithArgs(
        HttpActivity(mConnectionInfo->GetOrigin(),
                     mConnectionInfo->OriginPort(),
                     mConnectionInfo->EndToEndSSL()),
        NS_HTTP_ACTIVITY_TYPE_SOCKET_TRANSPORT, static_cast<uint32_t>(status),
        PR_Now(), progress, ""_ns);
  }
}

bool NullHttpTransaction::IsDone() { return mIsDone; }

nsresult NullHttpTransaction::Status() { return mStatus; }

uint32_t NullHttpTransaction::Caps() { return mCaps; }

nsresult NullHttpTransaction::ReadSegments(nsAHttpSegmentReader* reader,
                                           uint32_t count,
                                           uint32_t* countRead) {
  *countRead = 0;
  mIsDone = true;
  return NS_BASE_STREAM_CLOSED;
}

nsresult NullHttpTransaction::WriteSegments(nsAHttpSegmentWriter* writer,
                                            uint32_t count,
                                            uint32_t* countWritten) {
  *countWritten = 0;
  return NS_BASE_STREAM_CLOSED;
}

uint32_t NullHttpTransaction::Http1xTransactionCount() { return 0; }

nsHttpRequestHead* NullHttpTransaction::RequestHead() {
  // We suport a requesthead at all so that a CONNECT tunnel transaction
  // can obtain a Host header from it, but we lazy-popualate that header.

  if (!mRequestHead) {
    mRequestHead = new nsHttpRequestHead();

    nsAutoCString hostHeader;
    nsCString host(mConnectionInfo->GetOrigin());
    nsresult rv = nsHttpHandler::GenerateHostPort(
        host, mConnectionInfo->OriginPort(), hostHeader);
    if (NS_SUCCEEDED(rv)) {
      rv = mRequestHead->SetHeader(nsHttp::Host, hostHeader);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (mActivityDistributor) {
        // Report request headers.
        nsCString reqHeaderBuf;
        mRequestHead->Flatten(reqHeaderBuf, false);
        Unused << mActivityDistributor->ObserveActivityWithArgs(
            HttpActivity(mConnectionInfo->GetOrigin(),
                         mConnectionInfo->OriginPort(),
                         mConnectionInfo->EndToEndSSL()),
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_HEADER, PR_Now(), 0, reqHeaderBuf);
      }
    }

    // CONNECT tunnels may also want Proxy-Authorization but that is a lot
    // harder to determine, so for now we will let those connections fail in
    // the NullHttpTransaction and let them be retried from the pending queue
    // with a bound transaction
  }

  return mRequestHead;
}

nsresult NullHttpTransaction::TakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction> >& outTransactions) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void NullHttpTransaction::SetProxyConnectFailed() {}

void NullHttpTransaction::Close(nsresult reason) {
  mStatus = reason;
  mConnection = nullptr;
  mIsDone = true;
  if (mActivityDistributor) {
    // Report that this transaction is closing.
    Unused << mActivityDistributor->ObserveActivityWithArgs(
        HttpActivity(mConnectionInfo->GetOrigin(),
                     mConnectionInfo->OriginPort(),
                     mConnectionInfo->EndToEndSSL()),
        NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
        NS_HTTP_ACTIVITY_SUBTYPE_TRANSACTION_CLOSE, PR_Now(), 0, ""_ns);
  }
}

nsHttpConnectionInfo* NullHttpTransaction::ConnectionInfo() {
  return mConnectionInfo;
}

}  // namespace net
}  // namespace mozilla
