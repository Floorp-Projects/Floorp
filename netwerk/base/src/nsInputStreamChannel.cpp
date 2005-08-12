/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsInputStreamChannel.h"
#include "nsIServiceManager.h"
#include "nsIStreamTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransport.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "prlog.h"


#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsStreamChannel:5
//
static PRLogModuleInfo *gStreamChannelLog = nsnull;
#endif

#define LOG(args)     PR_LOG(gStreamChannelLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gStreamChannelLog, 4)

//-----------------------------------------------------------------------------
// nsInputStreamChannel methods
//-----------------------------------------------------------------------------

nsInputStreamChannel::nsInputStreamChannel()
    : mContentLength(-1)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
{
#if defined(PR_LOGGING)
    if (!gStreamChannelLog)
        gStreamChannelLog = PR_NewLogModule("nsStreamChannel");
#endif
}

nsInputStreamChannel::~nsInputStreamChannel()
{
}

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS5(nsInputStreamChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIInputStreamChannel)

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamChannel::GetName(nsACString &result)
{
    return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsInputStreamChannel::IsPending(PRBool *result)
{
    NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
    return mPump->IsPending(result);
}

NS_IMETHODIMP
nsInputStreamChannel::GetStatus(nsresult *status)
{
    if (mPump && NS_SUCCEEDED(mStatus))
        mPump->GetStatus(status);
    else
        *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Cancel(nsresult status)
{
    mStatus = status;

    if (mPump)
        mPump->Cancel(status);

    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Suspend()
{
    NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
    return mPump->Suspend();
}

NS_IMETHODIMP
nsInputStreamChannel::Resume()
{
    NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
    return mPump->Resume();
}

NS_IMETHODIMP
nsInputStreamChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsIChannel implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamChannel::GetOriginalURI(nsIURI **aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetOriginalURI(nsIURI *aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetURI(nsIURI **aURI)
{
    NS_IF_ADDREF(*aURI = mURI);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetOwner(nsISupports **aOwner)
{
    NS_IF_ADDREF(*aOwner = mOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetOwner(nsISupports *aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    NS_IF_ADDREF(*aCallbacks = mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
    mCallbacks = aCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
nsInputStreamChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentType(nsACString &aContentType)
{
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentType(const nsACString &aContentType)
{
    // If someone gives us a type hint we should just use that type.
    // So we don't care when this is being called.

    // mContentCharset is unchanged if not parsed
    NS_ParseContentType(aContentType, mContentType, mContentCharset);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentCharset(const nsACString &aContentCharset)
{
    // If someone gives us a charset hint we should just use that charset.
    // So we don't care when this is being called.
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentLength(PRInt32 aContentLength)
{
    // XXX does this really make any sense at all?
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Open(nsIInputStream **result)
{
    NS_ENSURE_TRUE(mContentStream, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);

    // XXX this won't work if mContentStream is non-blocking.

    NS_ADDREF(*result = mContentStream);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    NS_ENSURE_TRUE(mContentStream, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);

    // Initialize mProgressSink
    NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup, mProgressSink);

    // if content length is unknown, then we must guess... in this case, we
    // assume the stream can tell us.  if the stream is a pipe, then this will
    // not work.  in that case, we hope that the user of this interface would
    // have set our content length to PR_UINT32_MAX to cause us to read until
    // end of stream.
    if (mContentLength == -1)
        mContentStream->Available((PRUint32 *) &mContentLength);

    // XXX Once nsIChannel and nsIInputStream (in some future revision)
    // support 64-bit content lengths, mContentLength should be made 64-bit
    nsresult rv = NS_NewInputStreamPump(getter_AddRefs(mPump), mContentStream,
                                        nsInt64(-1), nsInt64(mContentLength),
                                        0, 0, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = mPump->AsyncRead(this, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    mListener = listener;
    mListenerContext = ctxt;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsIInputStreamChannel implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamChannel::SetURI(nsIURI *uri)
{
    NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);
    mURI = uri;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentStream(nsIInputStream **stream)
{
    NS_IF_ADDREF(*stream = mContentStream);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentStream(nsIInputStream *stream)
{
    NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);
    mContentStream = stream;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsIStreamListener implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamChannel::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsInputStreamChannel::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_SUCCEEDED(mStatus))
        mStatus = status;

    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    mPump = 0;
    mContentStream = 0;

    // Drop notification callbacks to prevent cycles.
    mCallbacks = 0;
    mProgressSink = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                                      nsIInputStream *stream,
                                      PRUint32 offset, PRUint32 count)
{
    nsresult rv;

    rv = mListener->OnDataAvailable(this, mListenerContext, stream, offset, count);

    // XXX need real 64-bit math here! (probably needs new streamlistener and
    // channel ifaces)
    if (mProgressSink && NS_SUCCEEDED(rv) && !(mLoadFlags & LOAD_BACKGROUND))
        mProgressSink->OnProgress(this, nsnull, nsUint64(offset + count),
                                  nsUint64(mContentLength));

    return rv; // let the pump cancel on failure
}
