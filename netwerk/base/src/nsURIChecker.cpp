/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Akkana Peck <akkana@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsURIChecker.h"

#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsString.h"
#include "nsReadableUtils.h"  // for ToNewUnicode()

//Interfaces for addref, release and queryinterface
NS_IMPL_ISUPPORTS3(nsURIChecker, nsIURIChecker,
                   nsIRequest, nsIStreamListener);

nsURIChecker::nsURIChecker()
{
    NS_INIT_REFCNT();
    mStatus = NS_OK;
    mIsPending = PR_FALSE;
}

nsURIChecker::~nsURIChecker()
{
}

void
nsURIChecker::SetStatusAndCallBack(nsresult aStatus)
{
    mStatus = aStatus;
    mIsPending = PR_FALSE;

    mObserver->OnStartRequest(NS_STATIC_CAST(nsIRequest*, this), mCtxt);
    mObserver->OnStopRequest(NS_STATIC_CAST(nsIRequest*, this),
                             mCtxt, mStatus);
}

/////////////////////////////////////////////////////
// nsIURIChecker methods
//
NS_IMETHODIMP
nsURIChecker::AsyncCheckURI(const char* aURI, nsIRequestObserver *aObserver,
                            nsISupports* aCtxt,
                            nsIRequest** aRequestRet)
{
    nsresult rv;

    mStatus = NS_OK;
    mIsPending = PR_TRUE;
    mStatus = NS_BINDING_REDIRECTED;
    mObserver = aObserver;
    mCtxt = aCtxt;
    if (aRequestRet)
        *aRequestRet = this;

    // Get the IO Service:
    nsCOMPtr<nsIIOService> ios (do_GetIOService(&rv));
    if (NS_FAILED(rv)) return rv;
    if (!ios) return NS_ERROR_UNEXPECTED;

    // Make the URI
    nsCOMPtr<nsIURI> URI;
    rv = ios->NewURI(aURI, 0, getter_AddRefs(URI));
    if (NS_FAILED(rv)) return rv;

    // Make a new channel:
    rv = ios->NewChannelFromURI(URI, getter_AddRefs(mChannel));
    if (NS_FAILED(rv)) return rv;

    // See if it's an http channel, which needs special treatment:
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
    if (httpChannel)
        httpChannel->SetRequestMethod("HEAD");

    // and start the request:
    return mChannel->AsyncOpen(this, nsnull);
}

/////////////////////////////////////////////////////
// nsIRequest methods
//
NS_IMETHODIMP
nsURIChecker::GetName(PRUnichar** aName)
{
    return mChannel->GetName(aName);
}

NS_IMETHODIMP
nsURIChecker::IsPending(PRBool *aPendingRet)
{
    *aPendingRet = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsURIChecker::GetStatus(nsresult* aStatusRet)
{
    NS_ENSURE_ARG(aStatusRet);
    *aStatusRet = mStatus;
    return NS_OK;
}

NS_IMETHODIMP nsURIChecker::Cancel(nsresult status)
{
    return mChannel->Cancel(status);
}

NS_IMETHODIMP nsURIChecker::Suspend()
{
    return mChannel->Suspend();
}

NS_IMETHODIMP nsURIChecker::Resume()
{
    return mChannel->Resume();
}

NS_IMETHODIMP nsURIChecker::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    return mChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP nsURIChecker::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    return mChannel->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP nsURIChecker::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    return mChannel->GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP nsURIChecker::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return mChannel->SetLoadFlags(aLoadFlags);
}

/////////////////////////////////////////////////////
// nsIStreamListener methods:
//
NS_IMETHODIMP
nsURIChecker::OnStartRequest(nsIRequest *aRequest, nsISupports *aCtxt)
{
    // DNS errors and other obvious problems will return failure status
    nsresult status;
    nsresult rv = aRequest->GetStatus(&status);
    if (NS_FAILED(rv) || NS_FAILED(status)) {
        SetStatusAndCallBack(NS_BINDING_FAILED);
        return NS_OK;
    }

    // We don't want to read the actual data, so cancel now:
    aRequest->Cancel(NS_BINDING_ABORTED);

    // If status is zero, it might still be an error if it's http:
    // http has data even when there's an error like a 404.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    if (!httpChannel) {
        SetStatusAndCallBack(NS_BINDING_SUCCEEDED);
        return NS_OK;
    }
    PRUint32 responseStatus;
    rv = httpChannel->GetResponseStatus(&responseStatus);
    if (NS_FAILED(rv)) {
        SetStatusAndCallBack(NS_BINDING_FAILED);
        return NS_OK;
    }
    // If it's between 200-299, it's valid:
    if (responseStatus / 100 == 2) {
        SetStatusAndCallBack(NS_BINDING_SUCCEEDED);
        return NS_OK;
    }
    // If we got a 404 (not found), we need some extra checking:
    // toplevel urls from Netscape Enterprise Server 3.6,
    // like http://www.mozilla.org, generate a 404
    // and will have to be retried without the head.
    if (responseStatus == 404)
    {
        char* server = 0;
        rv = httpChannel->GetResponseHeader("Server", &server);
        if (NS_SUCCEEDED(rv)) {
            if (!PL_strcasecmp(server, "Netscape-Enterprise/3.6")) {
                mStatus = NS_OK;
                // Open a new channel for a real (not head) request:
                nsCOMPtr<nsIIOService> ios (do_GetIOService(&rv));
                if (NS_FAILED(rv)) return rv;
                if (!ios) return NS_ERROR_UNEXPECTED;
                nsCOMPtr<nsIURI> URI;
                rv = mChannel->GetOriginalURI(getter_AddRefs(URI));
                if (NS_FAILED(rv)) return rv;
                rv = ios->NewChannelFromURI(URI, getter_AddRefs(mChannel));
                if (NS_FAILED(rv)) return rv;
                return mChannel->AsyncOpen(this, nsnull);
            }
        }

        // Else it was a normal 404, so return as expected
        SetStatusAndCallBack(NS_BINDING_FAILED);
        return NS_OK;
    }

    // If we get here, then it's an http channel, not a 100, 200 or 404.
    // Treat it as an error.
    SetStatusAndCallBack(NS_BINDING_FAILED);
    return NS_OK;
}

NS_IMETHODIMP
nsURIChecker::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult statusCode)
{
    return NS_OK;
}

// OnDataAvailable shouldn't generally be called,
// since we use head requests whenever possible,
// but in practice we're seeing it every time.
NS_IMETHODIMP
nsURIChecker::OnDataAvailable(nsIRequest *aRequest, nsISupports *aCtxt,
                               nsIInputStream *aInput, PRUint32 aOffset,
                               PRUint32 aCount)
{
#ifdef DEBUG_akkana
    PRUnichar* uri;
    GetName(&uri);
    nsString uristr(uri);
    char* cstr = ToNewCString(uristr);
    printf("OnDataAvailable: %s\n", cstr);
    Recycle(cstr);
#endif
    // If we've gotten here, something went wrong with the previous cancel,
    // so return a failure code to cancel the request:
    return NS_BINDING_ABORTED;
}

