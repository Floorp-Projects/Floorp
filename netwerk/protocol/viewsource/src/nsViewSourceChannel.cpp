 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com>
 */

#include "nsViewSourceChannel.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"

// nsViewSourceChannel methods
nsViewSourceChannel::nsViewSourceChannel()
{
    NS_INIT_REFCNT();
}

nsViewSourceChannel::~nsViewSourceChannel()
{
}


NS_IMPL_THREADSAFE_ADDREF(nsViewSourceChannel)
NS_IMPL_THREADSAFE_RELEASE(nsViewSourceChannel)
/*
  This QI uses hand-expansions of NS_INTERFACE_MAP_ENTRY to check for
  non-nullness of mHttpChannel and mCachingChannel.

  This seems like a better approach than writing out the whole QI by hand.
*/
NS_INTERFACE_MAP_BEGIN(nsViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY(nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  if ( mHttpChannel && aIID.Equals(NS_GET_IID(nsIHttpChannel)) )
    foundInterface = NS_STATIC_CAST(nsIHttpChannel*, this);
  else
  if ( mCachingChannel && aIID.Equals(NS_GET_IID(nsICachingChannel)) )
    foundInterface = NS_STATIC_CAST(nsICachingChannel*, this);
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequest, nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIChannel, nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIViewSourceChannel)
NS_INTERFACE_MAP_END_THREADSAFE

nsresult
nsViewSourceChannel::Init(nsIURI* uri)
{
    nsresult rv;

    nsXPIDLCString path;
    rv = uri->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIIOService> pService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
   
    rv = pService->NewChannel(path, nsnull, getter_AddRefs(mChannel));
    mHttpChannel = do_QueryInterface(mChannel);
    mCachingChannel = do_QueryInterface(mChannel);
    
    return rv;
}

NS_METHOD
nsViewSourceChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsViewSourceChannel* fc = new nsViewSourceChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsViewSourceChannel::GetName(PRUnichar* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsViewSourceChannel::IsPending(PRBool *result)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->IsPending(result);
}

NS_IMETHODIMP
nsViewSourceChannel::GetStatus(nsresult *status)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetStatus(status);
}

NS_IMETHODIMP
nsViewSourceChannel::Cancel(nsresult status)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->Cancel(status);
}

NS_IMETHODIMP
nsViewSourceChannel::Suspend(void)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->Suspend();
}

NS_IMETHODIMP
nsViewSourceChannel::Resume(void)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->Resume();
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsViewSourceChannel::GetOriginalURI(nsIURI* *aURI)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetOriginalURI(aURI);
}

NS_IMETHODIMP
nsViewSourceChannel::SetOriginalURI(nsIURI* aURI)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetOriginalURI(aURI);
}

NS_IMETHODIMP
nsViewSourceChannel::GetURI(nsIURI* *aURI)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetURI(aURI);
}

NS_IMETHODIMP
nsViewSourceChannel::Open(nsIInputStream **_retval)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->Open(_retval);
}

NS_IMETHODIMP
nsViewSourceChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    mListener = aListener;

    return mChannel->AsyncOpen(this, ctxt);
}

NS_IMETHODIMP
nsViewSourceChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsViewSourceChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    // "View source" always wants the currently cached content. 
 
    return mChannel->SetLoadFlags(aLoadFlags | nsIRequest::LOAD_FROM_CACHE);
}

#define X_VIEW_SOURCE_PARAM "; x-view-type=view-source"

NS_IMETHODIMP
nsViewSourceChannel::GetContentType(char* *aContentType) 
{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    if(mContentType.IsEmpty())
    {
        // Get the current content type
        nsresult rv;
        nsXPIDLCString contentType;
        rv = mChannel->GetContentType(getter_Copies(contentType));
        if (NS_FAILED(rv)) return rv;

        // Tack on the view-source param to the content type
        // Doing this so as to preserve the original content
        // type as received from the webserver. But, by adding
        // the x-view-type param we're indicating a custom preference
        // of how this should be displayed - viewsource or regular view

        nsCAutoString viewSrcContentType;
        viewSrcContentType.Append(contentType);
        viewSrcContentType.Append(X_VIEW_SOURCE_PARAM);

        // At this stage the content-type string will be
        // of the form "text/html; x-view-type=view-source"

        *aContentType = nsCRT::strdup(viewSrcContentType.get());

        if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;

        mContentType = *aContentType;

        return NS_OK;
    }
    else
    {
        *aContentType = mContentType.ToNewCString();

        if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;

        return NS_OK;
    }
}

NS_IMETHODIMP
nsViewSourceChannel::SetContentType(const char *aContentType)
{
    NS_ENSURE_ARG(aContentType);

    // Our GetContentType() currently returns strings of the 
    // form "text/html; x-view-type=view-source"(see above)
    //
    // However, during the parsing phase the parser calls our channels'
    // GetContentType(). Returing a string of the form given above
    // trips up the parser. In order to avoid messy changes and not to have
    // the parser depend on nsIViewSourceChannel Vidur proposed the 
    // following solution:
    //
    // The ViewSourceChannel initially returns a content type of the
    // form "text/html; x-view-type=view-source". Based on this type
    // decisions to create a viewer for doing a view source are made.
    // After the viewer is created, nsLayoutDLF::CreateInstance()
    // calls this SetContentType() with the original content type.
    // When it's time for the parser to find out the content type it
    // will call our channels' GetContentType() and it will get the 
    // original content type, such as, text/html and everything
    // is kosher from then on

    mContentType = aContentType;

    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::GetContentLength(PRInt32 *aContentLength)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
nsViewSourceChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetContentLength(aContentLength);
}

NS_IMETHODIMP
nsViewSourceChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsViewSourceChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsViewSourceChannel::GetOwner(nsISupports* *aOwner)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
nsViewSourceChannel::SetOwner(nsISupports* aOwner)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
nsViewSourceChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetNotificationCallbacks(aNotificationCallbacks);
}

NS_IMETHODIMP
nsViewSourceChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetNotificationCallbacks(aNotificationCallbacks);
}

NS_IMETHODIMP 
nsViewSourceChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetSecurityInfo(aSecurityInfo);
}

// nsIViewSourceChannel methods
NS_IMETHODIMP
nsViewSourceChannel::GetOriginalContentType(char* *aContentType) 
{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentType(aContentType);
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsViewSourceChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    return mListener->OnStartRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                    this),
                                     aContext);
}


NS_IMETHODIMP
nsViewSourceChannel::OnStopRequest(nsIRequest *aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    return mListener->OnStopRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                   this),
                                    aContext, aStatus);
}


// nsIStreamListener methods
NS_IMETHODIMP
nsViewSourceChannel::OnDataAvailable(nsIRequest *aRequest, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) 
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    return mListener->OnDataAvailable(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                     this),
                                      aContext, aInputStream,
                                      aSourceOffset, aLength);
}
