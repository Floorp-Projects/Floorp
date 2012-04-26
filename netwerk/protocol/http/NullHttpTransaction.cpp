/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  return true;
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

