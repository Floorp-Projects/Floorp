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

#include "prenv.h"

#include "nsIFile.h"
#include "nsIServiceManager.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "ipcSocketProviderUnix.h"
#include "nsISocketTransportService.h"
#include "nsEventQueueUtils.h"
#include "nsNetCID.h"
#include "nsNetError.h"
#include "nsCOMPtr.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcTransportUnix.h"
#include "ipcTransport.h"
#include "ipcm.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

#define IPC_BUFFER_SEGMENT_SIZE 4096

//-----------------------------------------------------------------------------
// ipcTransport (XP_UNIX specific methods)
//-----------------------------------------------------------------------------

nsresult
ipcTransport::InitUnix()
{
    nsresult rv = GetSocketPath(mSocketPath);
    if (NS_FAILED(rv)) return rv;

    ipcSocketProviderUnix::SetSocketPath(mSocketPath);
    return NS_OK;
}

nsresult
ipcTransport::Shutdown()
{
    LOG(("ipcTransport::Shutdown\n"));

    mHaveConnection = PR_FALSE;

    if (mTransport) {
        mTransport->Close(NS_BINDING_ABORTED);
        mTransport = nsnull;
        mInputStream = nsnull;
        mOutputStream = nsnull;
    }
    return NS_OK;
}

static NS_METHOD ipcWriteMessage(nsIOutputStream *stream,
                                 void            *closure,
                                 char            *segment,
                                 PRUint32         offset,
                                 PRUint32         count,
                                 PRUint32        *countWritten)
{
    ipcMessage *msg = (ipcMessage *) closure;

    PRBool complete;
    PRStatus rv = msg->WriteTo(segment, count, countWritten, &complete);
    NS_ASSERTION(rv == PR_SUCCESS, "failed to write message");

    // stop writing once the message has been completely written.
    if (*countWritten == 0 && complete)
        return NS_BASE_STREAM_CLOSED;

    return NS_OK;
}

nsresult
ipcTransport::SendMsg_Internal(ipcMessage *msg)
{
    LOG(("ipcTransport::SendMsg_Internal [dataLen=%u]\n", msg->DataLen()));

    NS_ENSURE_TRUE(mOutputStream, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    PRUint32 n;

    rv = mOutputStream->WriteSegments(ipcWriteMessage, msg, msg->MsgLen(), &n);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(n == msg->MsgLen(), "not all bytes written");

    delete msg; // done with message
    return NS_OK;
}

nsresult
ipcTransport::Connect()
{
    nsresult rv;

    LOG(("ipcTransport::Connect\n"));

    if (++mConnectionAttemptCount > 20) {
        LOG(("  giving up after 20 unsuccessful connection attempts\n"));
        return NS_ERROR_ABORT;
    }

    rv = CreateTransport();
    if (NS_FAILED(rv)) return rv; 

    //
    // send CLIENT_HELLO; expect CLIENT_ID in response.
    //
    rv = SendMsg_Internal(new ipcmMessageClientHello());
    if (NS_FAILED(rv)) return rv;

    //
    // put the receiver to work...
    //
    nsCOMPtr<nsIAsyncInputStream> asyncIn = do_QueryInterface(mInputStream, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQ;
    rv = NS_GetCurrentEventQ(getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    return asyncIn->AsyncWait(&mReceiver, 0, eventQ);
}

void
ipcTransport::OnConnectionLost(nsresult reason)
{
    LOG(("ipcTransport::OnConnectionLost [reason=%x]\n", reason));

    PRBool hadConnection = mHaveConnection;
    Shutdown();

    if (mObserver && hadConnection)
        mObserver->OnConnectionLost();

    if (reason == NS_BINDING_ABORTED)
        return;

    if (NS_FAILED(reason))
        OnConnectFailure();
}

nsresult
ipcTransport::CreateTransport()
{
    nsresult rv;
    nsCOMPtr<nsISocketTransportService> sts(
            do_GetService(kSocketTransportServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    const char *types[] = { IPC_SOCKET_TYPE };
    rv = sts->CreateTransport(types, 1,
                              NS_LITERAL_CSTRING("127.0.0.1"), IPC_PORT, nsnull,
                              getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    // open a blocking, buffered output stream (buffer size is unlimited)
    rv = mTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                      IPC_BUFFER_SEGMENT_SIZE, PRUint32(-1),
                                      getter_AddRefs(mOutputStream));
    if (NS_FAILED(rv)) return rv;

    // open a non-blocking, buffered input stream (buffer size limited)
    rv = mTransport->OpenInputStream(0, IPC_BUFFER_SEGMENT_SIZE, 4,
                                     getter_AddRefs(mInputStream));
    return rv;
}

nsresult
ipcTransport::GetSocketPath(nsACString &socketPath)
{
    nsCOMPtr<nsIFile> file;
    NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(file));
    if (!file)
        return NS_ERROR_FAILURE;

    const char *logName = PR_GetEnv("LOGNAME");
    if (!(logName && *logName)) {
        logName = PR_GetEnv("USER");
        if (!(logName && *logName)) {
            LOG(("could not determine username from environment\n"));
            return NS_ERROR_FAILURE;
        }
    }

    nsCAutoString leaf;
    leaf = NS_LITERAL_CSTRING(".mozilla-ipc-")
         + nsDependentCString(logName);
    file->AppendNative(leaf);
    file->AppendNative(NS_LITERAL_CSTRING("ipcd"));
    file->GetNativePath(socketPath);
    return NS_OK;
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

NS_IMPL_QUERY_INTERFACE1(ipcReceiver, nsIInputStreamNotify)

NS_METHOD
ipcReceiver::ReadSegment(nsIInputStream *stream,
                         void           *closure,
                         const char     *ptr,
                         PRUint32        offset,
                         PRUint32        count,
                         PRUint32       *countRead)
{
    ipcReceiver *self = (ipcReceiver *) closure;

    *countRead = 0;
    while (count) {
        PRUint32 nread;
        PRBool complete;

        self->mMsg.ReadFrom(ptr, count, &nread, &complete);

        if (complete) {
            self->mTransport->OnMessageAvailable(&self->mMsg);
            self->mMsg.Reset();
        }

        count -= nread;
        ptr += nread;
        *countRead += nread;
    }

    return NS_OK;
}

NS_IMETHODIMP
ipcReceiver::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    LOG(("ipcReceiver::OnInputStreamReady\n"));

    nsresult rv;
    PRUint32 n;

    rv = stream->ReadSegments(ReadSegment, this, IPC_BUFFER_SEGMENT_SIZE, &n);
    if (NS_FAILED(rv))
        mTransport->OnConnectionLost(rv);

    return NS_OK;
}
