/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
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
 *   Chak Nanga <chak@netscape.com>
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

#include "nsViewSourceChannel.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsIHttpHeaderVisitor.h"

NS_IMPL_ADDREF(nsViewSourceChannel)
NS_IMPL_RELEASE(nsViewSourceChannel)
/*
  This QI uses NS_INTERFACE_MAP_ENTRY_CONDITIONAL to check for
  non-nullness of mHttpChannel, mCachingChannel, and mUploadChannel.
*/
NS_INTERFACE_MAP_BEGIN(nsViewSourceChannel)
    NS_INTERFACE_MAP_ENTRY(nsIViewSourceChannel)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannel, mHttpChannel)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannelInternal, mHttpChannelInternal)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICachingChannel, mCachingChannel)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIUploadChannel, mUploadChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequest, nsIViewSourceChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIChannel, nsIViewSourceChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIViewSourceChannel)
NS_INTERFACE_MAP_END

nsresult
nsViewSourceChannel::Init(nsIURI* uri)
{
    mOriginalURI = uri;

    nsCAutoString path;
    nsresult rv = uri->GetPath(path);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIIOService> pService(do_GetIOService(&rv));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString scheme;
    rv = pService->ExtractScheme(path, scheme);
    if (NS_FAILED(rv))
      return rv;

    // prevent viewing source of javascript URIs (see bug 204779)
    if (scheme.LowerCaseEqualsLiteral("javascript")) {
      NS_WARNING("blocking view-source:javascript:");
      return NS_ERROR_INVALID_ARG;
    }

    rv = pService->NewChannel(path, nsnull, nsnull, getter_AddRefs(mChannel));
    if (NS_FAILED(rv))
      return rv;
 
    mChannel->SetOriginalURI(mOriginalURI);
    mHttpChannel = do_QueryInterface(mChannel);
    mHttpChannelInternal = do_QueryInterface(mChannel);
    mCachingChannel = do_QueryInterface(mChannel);
    mUploadChannel = do_QueryInterface(mChannel);
    
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsViewSourceChannel::GetName(nsACString &result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsViewSourceChannel::IsPending(bool *result)
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
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::SetOriginalURI(nsIURI* aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);
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

    nsresult rv = mChannel->Open(_retval);
    if (NS_SUCCEEDED(rv)) {
        mOpened = PR_TRUE;
    }
    
    return rv;
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
        loadGroup->AddRequest(static_cast<nsIViewSourceChannel*>
                                         (this), nsnull);
    
    nsresult rv = mChannel->AsyncOpen(this, ctxt);

    if (NS_FAILED(rv) && loadGroup)
        loadGroup->RemoveRequest(static_cast<nsIViewSourceChannel*>
                                            (this),
                                 nsnull, rv);

    if (NS_SUCCEEDED(rv)) {
        mOpened = PR_TRUE;
    }
    
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
    // channel's GetContentType(). Returning the string above trips up
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

    if (!mOpened) {
        // We do not take hints
        return NS_ERROR_NOT_AVAILABLE;
    }
    
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
nsViewSourceChannel::GetContentDisposition(PRUint32 *aContentDisposition)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
nsViewSourceChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentDispositionFilename(aContentDispositionFilename);
}

NS_IMETHODIMP
nsViewSourceChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
    NS_ENSURE_TRUE(mChannel, NS_ERROR_FAILURE);

    return mChannel->GetContentDispositionHeader(aContentDispositionHeader);
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
    
    return mListener->OnStartRequest(static_cast<nsIViewSourceChannel*>
                                                (this),
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
            loadGroup->RemoveRequest(static_cast<nsIViewSourceChannel*>
                                                (this),
                                     nsnull, aStatus);
        }
    }
    return mListener->OnStopRequest(static_cast<nsIViewSourceChannel*>
                                               (this),
                                    aContext, aStatus);
}


// nsIStreamListener methods
NS_IMETHODIMP
nsViewSourceChannel::OnDataAvailable(nsIRequest *aRequest, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) 
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_FAILURE);
    return mListener->OnDataAvailable(static_cast<nsIViewSourceChannel*>
                                                 (this),
                                      aContext, aInputStream,
                                      aSourceOffset, aLength);
}


// nsIHttpChannel methods

// We want to forward most of nsIHttpChannel over to mHttpChannel, but we want
// to override GetRequestHeader and VisitHeaders. The reason is that we don't
// want various headers like Link: and Refresh: applying to view-source.
NS_IMETHODIMP
nsViewSourceChannel::GetRequestMethod(nsACString & aRequestMethod)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetRequestMethod(aRequestMethod);
}

NS_IMETHODIMP
nsViewSourceChannel::SetRequestMethod(const nsACString & aRequestMethod)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetRequestMethod(aRequestMethod);
}

NS_IMETHODIMP
nsViewSourceChannel::GetReferrer(nsIURI * *aReferrer)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetReferrer(aReferrer);
}

NS_IMETHODIMP
nsViewSourceChannel::SetReferrer(nsIURI * aReferrer)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetReferrer(aReferrer);
}

NS_IMETHODIMP
nsViewSourceChannel::GetRequestHeader(const nsACString & aHeader,
                                      nsACString & aValue)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetRequestHeader(aHeader, aValue);
}

NS_IMETHODIMP
nsViewSourceChannel::SetRequestHeader(const nsACString & aHeader,
                                      const nsACString & aValue,
                                      bool aMerge)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetRequestHeader(aHeader, aValue, aMerge);
}

NS_IMETHODIMP
nsViewSourceChannel::VisitRequestHeaders(nsIHttpHeaderVisitor *aVisitor)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->VisitRequestHeaders(aVisitor);
}

NS_IMETHODIMP
nsViewSourceChannel::GetAllowPipelining(bool *aAllowPipelining)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetAllowPipelining(aAllowPipelining);
}

NS_IMETHODIMP
nsViewSourceChannel::SetAllowPipelining(bool aAllowPipelining)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetAllowPipelining(aAllowPipelining);
}

NS_IMETHODIMP
nsViewSourceChannel::GetRedirectionLimit(PRUint32 *aRedirectionLimit)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetRedirectionLimit(aRedirectionLimit);
}

NS_IMETHODIMP
nsViewSourceChannel::SetRedirectionLimit(PRUint32 aRedirectionLimit)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetRedirectionLimit(aRedirectionLimit);
}

NS_IMETHODIMP
nsViewSourceChannel::GetResponseStatus(PRUint32 *aResponseStatus)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetResponseStatus(aResponseStatus);
}

NS_IMETHODIMP
nsViewSourceChannel::GetResponseStatusText(nsACString & aResponseStatusText)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetResponseStatusText(aResponseStatusText);
}

NS_IMETHODIMP
nsViewSourceChannel::GetRequestSucceeded(bool *aRequestSucceeded)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->GetRequestSucceeded(aRequestSucceeded);
}

NS_IMETHODIMP
nsViewSourceChannel::GetResponseHeader(const nsACString & aHeader,
                                       nsACString & aValue)
{
    if (!mHttpChannel)
        return NS_ERROR_NULL_POINTER;

    if (!aHeader.Equals(NS_LITERAL_CSTRING("Content-Type"),
                        nsCaseInsensitiveCStringComparator()) &&
        !aHeader.Equals(NS_LITERAL_CSTRING("X-Content-Security-Policy"),
                        nsCaseInsensitiveCStringComparator()) &&
        !aHeader.Equals(NS_LITERAL_CSTRING("X-Content-Security-Policy-Report-Only"),
                        nsCaseInsensitiveCStringComparator()) &&
        !aHeader.Equals(NS_LITERAL_CSTRING("X-Frame-Options"),
                        nsCaseInsensitiveCStringComparator())) {
        aValue.Truncate();
        return NS_OK;
    }
        
    return mHttpChannel->GetResponseHeader(aHeader, aValue);
}

NS_IMETHODIMP
nsViewSourceChannel::SetResponseHeader(const nsACString & header,
                                       const nsACString & value, bool merge)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->SetResponseHeader(header, value, merge);
}

NS_IMETHODIMP
nsViewSourceChannel::VisitResponseHeaders(nsIHttpHeaderVisitor *aVisitor)
{
    if (!mHttpChannel)
        return NS_ERROR_NULL_POINTER;

    NS_NAMED_LITERAL_CSTRING(contentTypeStr, "Content-Type");
    nsCAutoString contentType;
    nsresult rv =
        mHttpChannel->GetResponseHeader(contentTypeStr, contentType);
    if (NS_SUCCEEDED(rv))
        aVisitor->VisitHeader(contentTypeStr, contentType);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceChannel::IsNoStoreResponse(bool *_retval)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->IsNoStoreResponse(_retval);
}

NS_IMETHODIMP
nsViewSourceChannel::IsNoCacheResponse(bool *_retval)
{
    return !mHttpChannel ? NS_ERROR_NULL_POINTER :
        mHttpChannel->IsNoCacheResponse(_retval);
} 
