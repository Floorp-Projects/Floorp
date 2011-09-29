/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akkana Peck <akkana@netscape.com> (original author)
 *   Darin Fisher <darin@meer.net>
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

#include "nsURIChecker.h"
#include "nsIServiceManager.h"
#include "nsIAuthPrompt.h"
#include "nsIHttpChannel.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsIAsyncVerifyRedirectCallback.h"

//-----------------------------------------------------------------------------

static bool
ServerIsNES3x(nsIHttpChannel *httpChannel)
{
    nsCAutoString server;
    httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Server"), server);
    // case sensitive string comparison is OK here.  the server string
    // is a well-known value, so we should not have to worry about it
    // being case-smashed or otherwise case-mutated.
    return StringBeginsWith(server,
                            NS_LITERAL_CSTRING("Netscape-Enterprise/3."));
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS6(nsURIChecker,
                   nsIURIChecker,
                   nsIRequest,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor)

nsURIChecker::nsURIChecker()
    : mStatus(NS_OK)
    , mIsPending(PR_FALSE)
    , mAllowHead(PR_TRUE)
{
}

void
nsURIChecker::SetStatusAndCallBack(nsresult aStatus)
{
    mStatus = aStatus;
    mIsPending = PR_FALSE;

    if (mObserver) {
        mObserver->OnStartRequest(this, mObserverContext);
        mObserver->OnStopRequest(this, mObserverContext, mStatus);
        mObserver = nsnull;
        mObserverContext = nsnull;
    }
}

nsresult
nsURIChecker::CheckStatus()
{
    NS_ASSERTION(mChannel, "no channel");

    nsresult status;
    nsresult rv = mChannel->GetStatus(&status);
    // DNS errors and other obvious problems will return failure status
    if (NS_FAILED(rv) || NS_FAILED(status))
        return NS_BINDING_FAILED;

    // If status is zero, it might still be an error if it's http:
    // http has data even when there's an error like a 404.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
    if (!httpChannel)
        return NS_BINDING_SUCCEEDED;

    PRUint32 responseStatus;
    rv = httpChannel->GetResponseStatus(&responseStatus);
    if (NS_FAILED(rv))
        return NS_BINDING_FAILED;

    // If it's between 200-299, it's valid:
    if (responseStatus / 100 == 2)
        return NS_BINDING_SUCCEEDED;

    // If we got a 404 (not found), we need some extra checking:
    // toplevel urls from Netscape Enterprise Server 3.6, like the old AOL-
    // hosted http://www.mozilla.org, generate a 404 and will have to be
    // retried without the head.
    if (responseStatus == 404) {
        if (mAllowHead && ServerIsNES3x(httpChannel)) {
            mAllowHead = PR_FALSE;

            // save the current value of mChannel in case we can't issue
            // the new request for some reason.
            nsCOMPtr<nsIChannel> lastChannel = mChannel;

            nsCOMPtr<nsIURI> uri;
            PRUint32 loadFlags;

            rv  = lastChannel->GetOriginalURI(getter_AddRefs(uri));
            rv |= lastChannel->GetLoadFlags(&loadFlags);

            // XXX we are carrying over the load flags, but what about other
            // parameters that may have been set on lastChannel??

            if (NS_SUCCEEDED(rv)) {
                rv = Init(uri);
                if (NS_SUCCEEDED(rv)) {
                    rv = mChannel->SetLoadFlags(loadFlags);
                    if (NS_SUCCEEDED(rv)) {
                        rv = AsyncCheck(mObserver, mObserverContext);
                        // if we succeeded in loading the new channel, then we
                        // want to return without notifying our observer.
                        if (NS_SUCCEEDED(rv))
                            return NS_BASE_STREAM_WOULD_BLOCK;
                    }
                }
            }
            // it is important to update this so our observer will be able
            // to access our baseChannel attribute if they want.
            mChannel = lastChannel;
        }
    }

    // If we get here, assume the resource does not exist.
    return NS_BINDING_FAILED;
}

//-----------------------------------------------------------------------------
// nsIURIChecker methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::Init(nsIURI *aURI)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    rv = ios->NewChannelFromURI(aURI, getter_AddRefs(mChannel));
    if (NS_FAILED(rv)) return rv;

    if (mAllowHead) {
        mAllowHead = PR_FALSE;
        // See if it's an http channel, which needs special treatment:
        nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
        if (httpChannel) {
            // We can have an HTTP channel that has a non-HTTP URL if
            // we're doing FTP via an HTTP proxy, for example.  See for
            // example bug 148813
            bool isReallyHTTP = false;
            aURI->SchemeIs("http", &isReallyHTTP);
            if (!isReallyHTTP)
                aURI->SchemeIs("https", &isReallyHTTP);
            if (isReallyHTTP) {
                httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("HEAD"));
                // set back to true so we'll know that this request is issuing
                // a HEAD request.  this is used down in OnStartRequest to
                // handle cases where we need to repeat the request as a normal
                // GET to deal with server borkage.
                mAllowHead = PR_TRUE;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsURIChecker::AsyncCheck(nsIRequestObserver *aObserver,
                         nsISupports *aObserverContext)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);

    // Hook us up to listen to redirects and the like (this creates a reference
    // cycle!)
    mChannel->SetNotificationCallbacks(this);
    
    // and start the request:
    nsresult rv = mChannel->AsyncOpen(this, nsnull);
    if (NS_FAILED(rv))
        mChannel = nsnull;
    else {
        // ok, wait for OnStartRequest to fire.
        mIsPending = PR_TRUE;
        mObserver = aObserver;
        mObserverContext = aObserverContext;
    }
    return rv;
}

NS_IMETHODIMP
nsURIChecker::GetBaseChannel(nsIChannel **aChannel)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    NS_ADDREF(*aChannel = mChannel);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIRequest methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::GetName(nsACString &aName)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->GetName(aName);
}

NS_IMETHODIMP
nsURIChecker::IsPending(bool *aPendingRet)
{
    *aPendingRet = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsURIChecker::GetStatus(nsresult* aStatusRet)
{
    *aStatusRet = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsURIChecker::Cancel(nsresult status)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->Cancel(status);
}

NS_IMETHODIMP
nsURIChecker::Suspend()
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->Suspend();
}

NS_IMETHODIMP
nsURIChecker::Resume()
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->Resume();
}

NS_IMETHODIMP
nsURIChecker::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsURIChecker::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsURIChecker::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsURIChecker::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_NOT_INITIALIZED);
    return mChannel->SetLoadFlags(aLoadFlags);
}

//-----------------------------------------------------------------------------
// nsIRequestObserver methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::OnStartRequest(nsIRequest *aRequest, nsISupports *aCtxt)
{
    NS_ASSERTION(aRequest == mChannel, "unexpected request");

    nsresult rv = CheckStatus();
    if (rv != NS_BASE_STREAM_WOULD_BLOCK)
        SetStatusAndCallBack(rv);

    // cancel the request (we don't care to look at the data).
    return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
nsURIChecker::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult statusCode)
{
    // NOTE: we may have kicked off a subsequent request, so we should not do
    // any cleanup unless this request matches the one we are currently using.
    if (mChannel == request) {
        // break reference cycle between us and the channel (see comment in
        // AsyncCheckURI)
        mChannel = nsnull;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIStreamListener methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::OnDataAvailable(nsIRequest *aRequest, nsISupports *aCtxt,
                               nsIInputStream *aInput, PRUint32 aOffset,
                               PRUint32 aCount)
{
    NS_NOTREACHED("nsURIChecker::OnDataAvailable");
    return NS_BINDING_ABORTED;
}

//-----------------------------------------------------------------------------
// nsIInterfaceRequestor methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::GetInterface(const nsIID & aIID, void **aResult)
{
    if (mObserver && aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        nsCOMPtr<nsIInterfaceRequestor> req = do_QueryInterface(mObserver);
        if (req)
            return req->GetInterface(aIID, aResult);
    }
    return QueryInterface(aIID, aResult);
}

//-----------------------------------------------------------------------------
// nsIChannelEventSink methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsURIChecker::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                     nsIChannel *aNewChannel,
                                     PRUint32 aFlags,
                                     nsIAsyncVerifyRedirectCallback *callback)
{
    // We have a new channel
    mChannel = aNewChannel;
    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}
