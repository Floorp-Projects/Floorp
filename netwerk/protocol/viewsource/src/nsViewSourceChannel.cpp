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
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"

// nsViewSourceChannel methods
nsViewSourceChannel::nsViewSourceChannel() : mIsDocument(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsViewSourceChannel::~nsViewSourceChannel()
{
}


NS_IMPL_THREADSAFE_ADDREF(nsViewSourceChannel)
NS_IMPL_THREADSAFE_RELEASE(nsViewSourceChannel)
/*
  This QI uses hand-expansions of NS_INTERFACE_MAP_ENTRY to check for
  non-nullness of mHttpChannel, mCachingChannel, and mUploadChannel.

  This seems like a better approach than writing out the whole QI by hand.
*/
NS_INTERFACE_MAP_BEGIN(nsViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY(nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannel, mHttpChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICachingChannel, mCachingChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIUploadChannel, mUploadChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequest, nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIChannel, nsIViewSourceChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIViewSourceChannel)
NS_INTERFACE_MAP_END_THREADSAFE

nsresult
nsViewSourceChannel::Init(nsIURI* uri)
{
    mOriginalURI = uri;

    nsCAutoString path;
    nsresult rv = uri->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIIOService> pService(do_GetIOService(&rv));
    if (NS_FAILED(rv)) return rv;

    rv = pService->NewChannel(path, nsnull, nsnull, getter_AddRefs(mChannel));
    if (NS_FAILED(rv))
      return rv;
 
    mChannel->SetOriginalURI(mOriginalURI);
    mHttpChannel = do_QueryInterface(mChannel);
    mCachingChannel = do_QueryInterface(mChannel);
    mUploadChannel = do_QueryInterface(mChannel);
    
    return NS_OK;
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
nsViewSourceChannel::GetName(nsACString &result)
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
    NS_ASSERTION(aURI, "Null out param!");
    *aURI = mOriginalURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::GetURI(nsIURI* *aURI)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    nsCOMPtr<nsIURI> uri;
    nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv))
      return rv;

    // protect ourselves against broken channel implementations
    if (!uri) {
      NS_ERROR("inner channel returned NS_OK and a null URI");
      return NS_ERROR_UNEXPECTED;
    }

    nsCAutoString spec;
    uri->GetSpec(spec);

    /* XXX Gross hack -- NS_NewURI goes into an infinite loop on
       non-flat specs.  See bug 136980 */
    return NS_NewURI(aURI, nsCAutoString(NS_LITERAL_CSTRING("view-source:")+spec), nsnull);
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

    /*
     * We want to add ourselves to the loadgroup before opening
     * mChannel, since we want to make sure we're in the loadgroup
     * when mChannel finishes and fires OnStopRequest()
     */
    
    nsCOMPtr<nsILoadGroup> loadGroup;
    mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup)
        loadGroup->AddRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                             this), nsnull);
    
    nsresult rv = mChannel->AsyncOpen(this, ctxt);

    if (NS_FAILED(rv) && loadGroup)
        loadGroup->RemoveRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                this),
                                 nsnull, rv);

    return rv;
}

/*
 * Both the view source channel and mChannel are added to the
 * loadgroup.  There should never be more than one request in the
 * loadgroup that has LOAD_DOCUMENT_URI set.  The one that has this
 * flag set is the request whose URI is used to refetch the document,
 * so it better be the viewsource channel.
 *
 * Therefore, we need to make sure that
 * 1) The load flags on mChannel _never_ include LOAD_DOCUMENT_URI
 * 2) The load flags on |this| include LOAD_DOCUMENT_URI when it was
 *    set via SetLoadFlags (mIsDocument keeps track of this flag).
 */

NS_IMETHODIMP
nsViewSourceChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    nsresult rv = mChannel->GetLoadFlags(aLoadFlags);
    if (NS_FAILED(rv))
      return rv;

    // This should actually be just LOAD_DOCUMENT_URI but the win32 compiler
    // fails to deal due to amiguous inheritance.  nsIChannel::LOAD_DOCUMENT_URI
    // also fails; the Win32 compiler thinks that's supposed to be a method.
    if (mIsDocument)
      *aLoadFlags |= ::nsIChannel::LOAD_DOCUMENT_URI;

    return rv;
}

NS_IMETHODIMP
nsViewSourceChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    // "View source" always wants the currently cached content.
    // We also want to have _this_ channel, not mChannel to be the
    // 'document' channel in the loadgroup. 
 
    // These should actually be just LOAD_FROM_CACHE and LOAD_DOCUMENT_URI but
    // the win32 compiler fails to deal due to amiguous inheritance.
    // nsIChannel::LOAD_DOCUMENT_URI/nsIRequest::LOAD_FROM_CACHE also fails; the
    // Win32 compiler thinks that's supposed to be a method.
    mIsDocument = (aLoadFlags & ::nsIChannel::LOAD_DOCUMENT_URI) ? PR_TRUE : PR_FALSE;

    return mChannel->SetLoadFlags((aLoadFlags |
                                   ::nsIRequest::LOAD_FROM_CACHE) &
                                  ~::nsIChannel::LOAD_DOCUMENT_URI);
}

#define X_VIEW_SOURCE_PARAM "; x-view-type=view-source"

NS_IMETHODIMP
nsViewSourceChannel::GetContentType(nsACString &aContentType) 
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    aContentType.Truncate();

    if (mContentType.IsEmpty())
    {
        // Get the current content type
        nsresult rv;
        nsCAutoString contentType;
        rv = mChannel->GetContentType(contentType);
        if (NS_FAILED(rv)) return rv;

        // If we don't know our type, just say so.  The unknown
        // content decoder will then kick in automatically, and it
        // will call our SetOriginalContentType method instead of our
        // SetContentType method to set the type it determines.
        if (!contentType.Equals(UNKNOWN_CONTENT_TYPE)) {
          contentType = VIEWSOURCE_CONTENT_TYPE;
        }

        mContentType = contentType;
    }

    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::SetContentType(const nsACString &aContentType)
{
    // Our GetContentType() currently returns VIEWSOURCE_CONTENT_TYPE
    //
    // However, during the parsing phase the parser calls our
    // channel's GetContentType(). Returing the string above trips up
    // the parser. In order to avoid messy changes and not to have the
    // parser depend on nsIViewSourceChannel Vidur proposed the
    // following solution:
    //
    // The ViewSourceChannel initially returns a content type of
    // VIEWSOURCE_CONTENT_TYPE.  Based on this type decisions to
    // create a viewer for doing a view source are made.  After the
    // viewer is created, nsLayoutDLF::CreateInstance() calls this
    // SetContentType() with the original content type.  When it's
    // time for the parser to find out the content type it will call
    // our channel's GetContentType() and it will get the original
    // content type, such as, text/html and everything is kosher from
    // then on.

    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::GetContentCharset(nsACString &aContentCharset)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentCharset(aContentCharset);
}

NS_IMETHODIMP
nsViewSourceChannel::SetContentCharset(const nsACString &aContentCharset)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->SetContentCharset(aContentCharset);
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
nsViewSourceChannel::GetOriginalContentType(nsACString &aContentType) 
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentType(aContentType);
}

NS_IMETHODIMP
nsViewSourceChannel::SetOriginalContentType(const nsACString &aContentType)
{
  NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

  // clear our cached content-type value
  mContentType.Truncate();

  return mChannel->SetContentType(aContentType);
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsViewSourceChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    // The channel may have gotten redirected... Time to update our info
    mChannel = do_QueryInterface(aRequest);
    mHttpChannel = do_QueryInterface(aRequest);
    mCachingChannel = do_QueryInterface(aRequest);
    mUploadChannel = do_QueryInterface(aRequest);
    
    if (mHttpChannel) {
      // we don't want view-source following Refresh: headers, so clear it
      mHttpChannel->SetResponseHeader(NS_LITERAL_CSTRING("Refresh"),
                                      NS_LITERAL_CSTRING(""), PR_FALSE);
    }
    return mListener->OnStartRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                    this),
                                     aContext);
}


NS_IMETHODIMP
nsViewSourceChannel::OnStopRequest(nsIRequest *aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    if (mChannel)
    {
        nsCOMPtr<nsILoadGroup> loadGroup;
        mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup)
        {
            loadGroup->RemoveRequest(NS_STATIC_CAST(nsIViewSourceChannel*,
                                                    this),
                                     nsnull, aStatus);
        }
    }
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
