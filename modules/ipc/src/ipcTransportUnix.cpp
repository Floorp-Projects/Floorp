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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIInputStream.h"
#include "nsIOutputStream.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcTransportUnix.h"
#include "ipcTransport.h"

//-----------------------------------------------------------------------------
// ipcSendQueue
//-----------------------------------------------------------------------------

NS_IMETHODIMP_(nsrefcnt)
ipcSendQueue::AddRef()
{
    return mTransport->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
ipcSendQueue::Release()
{
    return mTransport->Release();
}

NS_IMPL_QUERY_INTERFACE2(ipcSendQueue, nsIStreamProvider, nsIRequestObserver)

NS_IMETHODIMP
ipcSendQueue::OnStartRequest(nsIRequest *request,
                             nsISupports *context)
{
    LOG(("ipcSendQueue::OnStartRequest\n"));

    if (mTransport)
        mTransport->OnStartRequest(request);

    return NS_OK;
}

NS_IMETHODIMP
ipcSendQueue::OnStopRequest(nsIRequest *request,
                            nsISupports *context,
                            nsresult status)
{
    LOG(("ipcSendQueue::OnStopRequest [status=%x]\n", status));

    if (mTransport)
        mTransport->OnStopRequest(request, status);

    return NS_OK;
}

struct ipcWriteState
{
    ipcMessage *msg;
    PRBool      complete;
};

static NS_METHOD ipcWriteMessage(nsIOutputStream *stream,
                                 void            *closure,
                                 char            *segment,
                                 PRUint32         offset,
                                 PRUint32         count,
                                 PRUint32        *countWritten)
{
    ipcWriteState *state = (ipcWriteState *) closure;

    if (state->msg->WriteTo(segment, count,
                            countWritten, &state->complete) != PR_SUCCESS)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

NS_IMETHODIMP
ipcSendQueue::OnDataWritable(nsIRequest *request,
                             nsISupports *context,
                             nsIOutputStream *stream,
                             PRUint32 offset,
                             PRUint32 count)
{
    PRUint32 n;
    nsresult rv;
    ipcWriteState state;
    PRBool wroteSomething = PR_FALSE;

    LOG(("ipcSendQueue::OnDataWritable\n"));

    while (!mQueue.IsEmpty()) {
        state.msg = mQueue.First();
        state.complete = PR_FALSE;

        rv = stream->WriteSegments(ipcWriteMessage, &state, count, &n);
        if (NS_FAILED(rv))
            break;

        if (state.complete) {
            LOG(("  wrote message %u bytes\n", mQueue.First()->MsgLen()));
            mQueue.DeleteFirst();
        }

        wroteSomething = PR_TRUE;
    }

    if (wroteSomething)
        return NS_OK;

    LOG(("  suspending write request\n"));

    mTransport->SetWriteSuspended(PR_TRUE);
    return NS_BASE_STREAM_WOULD_BLOCK;
}

//----------------------------------------------------------------------------
// ipcReceiver
//----------------------------------------------------------------------------

NS_IMETHODIMP_(nsrefcnt)
ipcReceiver::AddRef()
{
    return mTransport->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
ipcReceiver::Release()
{
    return mTransport->Release();
}

NS_IMPL_QUERY_INTERFACE2(ipcReceiver, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
ipcReceiver::OnStartRequest(nsIRequest *request,
                            nsISupports *context)
{
    LOG(("ipcReceiver::OnStartRequest\n"));

    if (mTransport)
        mTransport->OnStartRequest(request);

    return NS_OK;
}

NS_IMETHODIMP
ipcReceiver::OnStopRequest(nsIRequest *request,
                           nsISupports *context,
                           nsresult status)
{
    LOG(("ipcReceiver::OnStopRequest [status=%x]\n", status));

    if (mTransport)
        mTransport->OnStopRequest(request, status);

    return NS_OK;
}

static NS_METHOD ipcReadMessage(nsIInputStream *stream,
                                void           *closure,
                                const char     *segment,
                                PRUint32        offset,
                                PRUint32        count,
                                PRUint32       *countRead)
{
    ipcReceiver *receiver = (ipcReceiver *) closure;
    return receiver->ReadSegment(segment, count, countRead);
}

NS_IMETHODIMP
ipcReceiver::OnDataAvailable(nsIRequest *request,
                             nsISupports *context,
                             nsIInputStream *stream,
                             PRUint32 offset,
                             PRUint32 count)
{
    LOG(("ipcReceiver::OnDataAvailable [count=%u]\n", count));

    PRUint32 countRead;
    return stream->ReadSegments(ipcReadMessage, this, count, &countRead);
}

nsresult
ipcReceiver::ReadSegment(const char *ptr, PRUint32 count, PRUint32 *countRead)
{
    *countRead = 0;
    while (count) {
        PRUint32 nread;
        PRBool complete;

        mMsg.ReadFrom(ptr, count, &nread, &complete);

        if (complete) {
            mTransport->OnMessageAvailable(&mMsg);
            mMsg.Reset();
        }

        count -= nread;
        ptr += nread;
        *countRead += nread;
    }

    return NS_OK;
}
