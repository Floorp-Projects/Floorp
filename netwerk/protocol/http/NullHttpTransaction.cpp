/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "NullHttpTransaction.h"
#include "nsProxyRelease.h"
#include "nsHttpHandler.h"

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS0(NullHttpTransaction);

NullHttpTransaction::NullHttpTransaction(nsHttpConnectionInfo *ci,
                                         nsIInterfaceRequestor *callbacks,
                                         nsIEventTarget *target,
                                         PRUint8 caps)
  : mStatus(NS_OK)
  , mCaps(caps)
  , mCallbacks(callbacks)
  , mEventTarget(target)
  , mConnectionInfo(ci)
  , mRequestHead(nsnull)
  , mIsDone(false)
{
}

NullHttpTransaction::~NullHttpTransaction()
{
  if (mCallbacks) {
    nsIInterfaceRequestor *cbs = nsnull;
    mCallbacks.swap(cbs);
    NS_ProxyRelease(mEventTarget, cbs);
  }
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
NullHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **outCB,
                                           nsIEventTarget **outTarget)
{
  nsCOMPtr<nsIInterfaceRequestor> copyCB(mCallbacks);
  *outCB = copyCB;
  copyCB.forget();

  if (outTarget) {
    nsCOMPtr<nsIEventTarget> copyET(mEventTarget);
    *outTarget = copyET;
    copyET.forget();
  }
}

void
NullHttpTransaction::OnTransportStatus(nsITransport* transport,
                                       nsresult status, PRUint64 progress)
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

PRUint8
NullHttpTransaction::Caps()
{
  return mCaps;
}

PRUint32
NullHttpTransaction::Available()
{
  return 0;
}

nsresult
NullHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                  PRUint32 count, PRUint32 *countRead)
{
  *countRead = 0;
  mIsDone = true;
  return NS_BASE_STREAM_CLOSED;
}

nsresult
NullHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                   PRUint32 count, PRUint32 *countWritten)
{
  *countWritten = 0;
  return NS_BASE_STREAM_CLOSED;
}

PRUint32
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

    nsCAutoString hostHeader;
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
NullHttpTransaction::SetSSLConnectFailed()
{
}

void
NullHttpTransaction::Close(nsresult reason)
{
  mStatus = reason;
  mConnection = nsnull;
  mIsDone = true;
}

nsresult
NullHttpTransaction::AddTransaction(nsAHttpTransaction *trans)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

PRUint32
NullHttpTransaction::PipelineDepth()
{
  return 0;
}

nsresult
NullHttpTransaction::SetPipelinePosition(PRInt32 position)
{
    return NS_OK;
}
 
PRInt32
NullHttpTransaction::PipelinePosition()
{
  return 1;
}

} // namespace mozilla::net
} // namespace mozilla

