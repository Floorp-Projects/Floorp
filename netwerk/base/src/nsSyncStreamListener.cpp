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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

#include "nsIOService.h"
#include "nsSyncStreamListener.h"
#include "nsIPipe.h"

nsresult
nsSyncStreamListener::Init()
{
    return NS_NewPipe(getter_AddRefs(mPipeIn),
                      getter_AddRefs(mPipeOut),
                      nsIOService::gDefaultSegmentSize,
                      PR_UINT32_MAX, // no size limit
                      PR_FALSE,
                      PR_FALSE);
}

nsresult
nsSyncStreamListener::WaitForData()
{
    mKeepWaiting = PR_TRUE;

    while (mKeepWaiting)
        NS_ENSURE_STATE(NS_ProcessNextEvent(NS_GetCurrentThread()));

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsSyncStreamListener,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIInputStream,
                   nsISyncStreamListener)

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsISyncStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::GetInputStream(nsIInputStream **result)
{
    NS_ADDREF(*result = this);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::OnStartRequest(nsIRequest  *request,
                                     nsISupports *context)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::OnDataAvailable(nsIRequest     *request,
                                      nsISupports    *context,
                                      nsIInputStream *stream,
                                      PRUint32        offset,
                                      PRUint32        count)
{
    PRUint32 bytesWritten;

    nsresult rv = mPipeOut->WriteFrom(stream, count, &bytesWritten);

    // if we get an error, then return failure.  this will cause the
    // channel to be canceled, and as a result our OnStopRequest method
    // will be called immediately.  because of this we do not need to
    // set mStatus or mKeepWaiting here.
    if (NS_FAILED(rv))
        return rv;

    // we expect that all data will be written to the pipe because
    // the pipe was created to have "infinite" room.
    NS_ASSERTION(bytesWritten == count, "did not write all data"); 

    mKeepWaiting = PR_FALSE; // unblock Read
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::OnStopRequest(nsIRequest  *request,
                                    nsISupports *context,
                                    nsresult     status)
{
    mStatus = status;
    mKeepWaiting = PR_FALSE; // unblock Read
    mDone = PR_TRUE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsIInputStream
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;
    mDone = PR_TRUE;

    // It'd be nice if we could explicitly cancel the request at this point,
    // but we don't have a reference to it, so the best we can do is close the
    // pipe so that the next OnDataAvailable event will fail.
    if (mPipeIn) {
        mPipeIn->Close();
        mPipeIn = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::Available(PRUint32 *result)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    mStatus = mPipeIn->Available(result);
    if (NS_SUCCEEDED(mStatus) && (*result == 0) && !mDone) {
        mStatus = WaitForData();
        if (NS_SUCCEEDED(mStatus))
            mStatus = mPipeIn->Available(result);
    }
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::Read(char     *buf,
                           PRUint32  bufLen,
                           PRUint32 *result)
{
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *result = 0;
        return NS_OK;
    }

    PRUint32 avail;
    if (NS_FAILED(Available(&avail)))
        return mStatus;

    avail = NS_MIN(avail, bufLen);
    mStatus = mPipeIn->Read(buf, avail, result);
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::ReadSegments(nsWriteSegmentFun  writer,
                                   void              *closure,
                                   PRUint32           count,
                                   PRUint32          *result)
{
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *result = 0;
        return NS_OK;
    }

    PRUint32 avail;
    if (NS_FAILED(Available(&avail)))
        return mStatus;

    avail = NS_MIN(avail, count);
    mStatus = mPipeIn->ReadSegments(writer, closure, avail, result);
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::IsNonBlocking(bool *result)
{
    *result = PR_FALSE;
    return NS_OK;
}
