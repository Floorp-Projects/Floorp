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

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NullHttpTransaction, NullHttpTransaction, nsISupportsWeakReference)

NullHttpTransaction::NullHttpTransaction(nsHttpConnectionInfo *ci,
                                         nsIInterfaceRequestor *callbacks,
                                         uint32_t caps)
  : mStatus(NS_OK)
  , mCaps(caps | NS_HTTP_ALLOW_KEEPALIVE)
  , mCapsToClear(0)
  , mCallbacks(callbacks)
  , mConnectionInfo(ci)
  , mRequestHead(nullptr)
  , mIsDone(false)
{
}

NullHttpTransaction::~NullHttpTransaction()
{
  mCallbacks = nullptr;
  delete mRequestHead;
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
                                       nsresult status, uint64_t progress)
{
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
    nsCString host(mConnectionInfo->GetHost());
    nsresult rv = nsHttpHandler::GenerateHostPort(host,
                                                  mConnectionInfo->Port(),
                                                  hostHeader);
    if (NS_SUCCEEDED(rv))
       mRequestHead->SetHeader(nsHttp::Host, hostHeader);

    // CONNECT tunnels may also want Proxy-Authorization but that is a lot
    // harder to determine, so for now we will let those connections fail in
    // the NullHttpTransaction and let them be retried from the pending queue
    // with a bound transcation
  }

  return mRequestHead;
}

nsresult
NullHttpTransaction::TakeSubTransactions(
  nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
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

} // namespace mozilla::net
} // namespace mozilla

